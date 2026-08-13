# Deploy na VPS (smartpark-red.sytes.net)

A VPS já hospeda vários outros projetos, todos atrás de um único `nginx-master`
(container `nginx-master`, config em `/mcorecloud/nginx-master/conf.d/*.conf`,
rede externa `mcorecloud-network`). Este projeto segue exatamente o mesmo padrão:
não altera nada dos demais, só soma mais um `conf.d/*.conf` e conecta seu container
Node-RED na rede `mcorecloud-network`.

## 1. Clonar e configurar

```bash
cd ~
git clone git@github.com:jorgyvanlima/smartpark-red.git
cd smartpark-red
cp .env.example .env
nano .env   # preencha PG_PASSWORD e NODE_RED_CREDENTIAL_SECRET com valores fortes
```

## 2. Subir os containers

```bash
./scripts/deploy.sh
```

Isso cria a rede `smartpark_internal`, sobe Mosquitto, Postgres (com schema/seed
automáticos via `postgres/init/*.sql`) e Node-RED, e já conecta o Node-RED também
na rede externa `mcorecloud-network` (definida como `external: true` no
`docker-compose.yml`, que já existe na VPS — não é recriada).

## 3. Vhost do Nginx + certificado SSL (primeira vez)

O `nginx-master` falha ao subir/recarregar se um `server { listen 443 ssl; }`
referenciar um certificado que ainda não existe — por isso o certificado é obtido
**antes** de habilitar o bloco HTTPS, exatamente como já foi feito para os outros
subdomínios `*.sytes.net` desta VPS.

```bash
# 3.1 — publica só o bloco HTTP (necessário para o desafio ACME)
cat > /mcorecloud/nginx-master/conf.d/smartpark-red.conf <<'EOF'
server {
    listen 80;
    server_name smartpark-red.sytes.net;

    location /.well-known/acme-challenge/ {
        root /var/www/certbot;
    }

    location / {
        return 301 https://$host$request_uri;
    }
}
EOF

docker exec nginx-master nginx -s reload

# 3.2 — emite o certificado (usa o certbot instalado no host, mesmo fluxo
# já usado para storm-mlsecops.sytes.net, erp.mcorecloud.com.br, etc.)
sudo certbot certonly --webroot \
    -w /mcorecloud/nginx-master/www \
    -d smartpark-red.sytes.net \
    --non-interactive --agree-tos -m jorgyvan.lima@gmail.com

# 3.3 — substitui pelo vhost final (HTTP redirect + HTTPS com proxy_pass)
cp ~/smartpark-red/nginx/smartpark-red.conf /mcorecloud/nginx-master/conf.d/smartpark-red.conf
docker exec nginx-master nginx -t
docker exec nginx-master nginx -s reload
```

A partir daqui, `https://smartpark-red.sytes.net` serve o Portal do Cliente e
`https://smartpark-red.sytes.net/api/vagas` a API — tudo via o mesmo Nginx que já
atende aos outros domínios da VPS, sem novas portas 80/443.

## 4. Firewall (porta MQTT)

O broker MQTT precisa ficar acessível diretamente (ESP32/Wokwi e apps mobile não
falam HTTP) — não dá pra passar por trás do Nginx sem um `stream{}` dedicado, então
ele usa uma porta própria, `1884`, para não colidir com o broker MQTT que já roda
na porta `1883` para outro projeto desta VPS:

```bash
sudo ufw allow 1884/tcp comment 'SmartPark-RED MQTT'
```

## 5. Configuração do ESP32 (Wokwi)

Projeto em `esp32/` (`sketch.ino`, `diagram.json`, `libraries.txt`, `wokwi.toml`).
No Wokwi:

- `WIFI_SSID = "Wokwi-GUEST"`
- `MQTT_HOST = "smartpark-red.sytes.net"`
- `MQTT_PORT = 1884`
- Tópico de publicação: `estacionamento/vagas/status`

## 6. App mobile (operador) — IoT MQTT Panel / MQTT Dash

| Campo             | Valor                          |
|---------------------|---------------------------------|
| Host                 | `smartpark-red.sytes.net`       |
| Porta                | `1884`                          |
| TLS                  | desligado (MQTT puro)           |
| Client ID            | qualquer valor único             |

Assinaturas recomendadas:

- `estacionamento/resumo` → widget "Cards"/"Texto" com os contadores livres/ocupadas.
- `estacionamento/alerta` → widget de notificação/push (`LOTADO`, vaga PCD ocupada).
- `estacionamento/vagas/status` → widget de log/histórico de eventos.
- `estacionamento/vagas/<ID>/status` → **um widget colorido por vaga** (ver seção 6.1).

> ⚠️ O app **myMQTT** (o "cliente" simples, só de log de texto) **não tem
> widgets coloridos** — ele só mostra mensagens como texto puro. Para ter os
> quadradinhos verde/vermelho/azul por vaga, use o **IoT MQTT Panel** ou o
> **MQTT Dash**.

### 6.1 Card colorido por vaga (IoT MQTT Panel)

Cada vaga publica seu próprio tópico retido `estacionamento/vagas/<ID>/status`
com um valor simples: `LIVRE`, `OCUPADA` ou `OCUPADA_PCD`. Para criar um card
por vaga:

1. Abra o IoT MQTT Panel → sua conexão (host `smartpark-red.sytes.net`,
   porta `1884`) → **"+"** → **New Panel** (crie um painel, ex: "Andar 1").
2. Dentro do painel, toque em **"+"** → escolha o widget **"LED"** (ou
   "Simple Text"/"Text View" se quiser mostrar o texto além da cor).
3. Configure o widget:
   - **Topic**: `estacionamento/vagas/A01/status` (troque `A01` pela vaga)
   - **Payload ON**: `OCUPADA` → cor vermelha
   - **Payload OFF**: `LIVRE` → cor verde
   - Para as vagas preferenciais (`A05`, `B05`, `C05`), adicione uma segunda
     regra de cor (se o app permitir múltiplos valores) para
     `OCUPADA_PCD` → azul; caso o widget só aceite 2 estados, use o widget
     **"Text"** com regra de cor por valor (`LIVRE`=verde, `OCUPADA`=vermelho,
     `OCUPADA_PCD`=azul).
4. Repita para as 15 vagas (`A01`...`A05`, `B01`...`B05`, `C01`...`C05`),
   organizando em 3 painéis (um por andar) para ficar visualmente igual ao
   estacionamento físico.
5. Como o tópico é retido, os cards já aparecem com a cor certa assim que
   você abre o app — não precisa esperar nenhuma vaga mudar de estado.

## 7. Editor do Node-RED (via navegador, com login)

O editor de flows fica em `https://smartpark-red.sytes.net/admin-red`, atrás do
mesmo Nginx/HTTPS do site e protegido por usuário/senha (`adminAuth` em
`nodered/settings.js`, lido de `NODE_RED_ADMIN_USER` /
`NODE_RED_ADMIN_PASSWORD_HASH` no `.env`). Não precisa de túnel SSH nem de
ambiente gráfico na VPS — só abrir a URL em qualquer navegador.

Para trocar a senha:

```bash
python3 -c "import bcrypt; print(bcrypt.hashpw(b'SUA_SENHA_NOVA', bcrypt.gensalt(10)).decode())"
```

Copie o hash gerado para `NODE_RED_ADMIN_PASSWORD_HASH` no `.env` (entre aspas
simples, pois o valor contém `$`) e rode `./scripts/deploy.sh` novamente.

## 8. Redeploy / atualização

```bash
cd ~/smartpark-red
git pull
./scripts/deploy.sh
```

## 9. Memória da VPS

A VPS já roda vários outros projetos e opera com pouca RAM livre. Os três serviços
deste projeto têm `mem_limit` conservador no `docker-compose.yml`
(Mosquitto 96MB, Postgres 256MB, Node-RED 320MB) para não competir demais com os
demais containers. Se notar OOM-kills (`dmesg | grep -i oom`), considere criar um
swapfile (`fallocate -l 2G /swapfile && mkswap /swapfile && swapon /swapfile`).
