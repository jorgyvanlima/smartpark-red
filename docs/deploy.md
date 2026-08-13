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

## 7. Editor do Node-RED (uso administrativo, não público)

O editor de flows (`/admin-red`) **não** é exposto pelo Nginx (bloco `return 404;`
no vhost). Para editar os flows, use um túnel SSH até a VPS:

```bash
ssh -L 1880:localhost:1880 -p 22022 jorgyvan@smartpark-red.sytes.net \
    -N -o ProxyCommand="none"
# depois, no navegador local: http://localhost:1880/admin-red
```

(ou, mais simples, `docker exec -it smartpark_nodered sh` para inspecionar/editar
`/data/flows.json` diretamente).

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
