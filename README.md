# SmartPark-RED

**Sistema IoT de estacionamento inteligente** — 3 andares, 15 vagas monitoradas em
tempo real, arquitetura MQTT *publish/subscribe* de dupla via (sensor físico +
simulador de backup), persistência em PostgreSQL, API REST, portal web e app
mobile — 100% em containers Docker, publicado em produção em
**[smartpark-red.sytes.net](https://smartpark-red.sytes.net)**.

> Projeto acadêmico da disciplina **Tecnologias de Comunicação em IoT**, Curso de
> Especialização em Sistemas de Segurança Integrada da Informação e
> Cibersegurança — **UFPA**, Belém-PA, 2026.

---

## Sumário

- [Visão geral](#visão-geral)
- [Arquitetura](#arquitetura)
- [Stack técnica](#stack-técnica)
- [Estrutura do repositório](#estrutura-do-repositório)
- [Modelo de dados](#modelo-de-dados)
- [Fluxos do Node-RED](#fluxos-do-node-red)
- [Tópicos MQTT](#tópicos-mqtt)
- [API REST](#api-rest)
- [Portal web do cliente](#portal-web-do-cliente)
- [Firmware ESP32 / Wokwi](#firmware-esp32--wokwi)
- [App mobile (operador)](#app-mobile-operador)
- [Segurança](#segurança)
- [Deploy](#deploy)
- [Limitações conhecidas e trabalhos futuros](#limitações-conhecidas-e-trabalhos-futuros)
- [Equipe](#equipe)

---

## Visão geral

Em estacionamentos de múltiplos andares, a falta de indicação de vagas livres em
tempo real gera circulação desnecessária de veículos, perda de tempo e emissão
extra de poluentes. O SmartPark-RED resolve isso monitorando 15 vagas (`A01`
a `C05`, distribuídas em 3 andares, com `A05`/`B05`/`C05` reservadas como
preferenciais/PCD) e distribuindo o status em tempo real para dois tipos de
consumidor: o **operador** do estacionamento (app MQTT no celular) e o
**cliente final** (portal web público).

A geração do dado tem **dupla via**, um requisito do escopo acadêmico do
projeto e também uma decisão de robustez:

1. **Sensor físico simulado** — um ESP32 no [Wokwi](https://wokwi.com/) lê 15
   botões (um por vaga) e publica cada mudança de estado no broker MQTT.
2. **Publisher de backup** — um flow do Node-RED sorteia uma vaga a cada 15s e
   inverte seu estado, publicando no mesmo tópico. Isso mantém o portal e o
   app funcionando mesmo com o simulador Wokwi desligado, e serve para testar
   o pipeline completo de forma independente do hardware.

## Arquitetura

```
                    ┌───────────────────────┐        ┌──────────────────────────┐
                    │   Wokwi (ESP32 C++)    │        │   Node-RED — Flow 2      │
                    │   15 botões + NeoPixel │        │   Simulador (a cada 15s) │
                    └───────────┬────────────┘        └────────────┬─────────────┘
                                │ publish                          │ publish
                                └────────────┬─────────────────────┘
                                             ▼
                            ┌─────────────────────────────────┐
                            │  Mosquitto (smartpark_mosquitto) │
                            │  :1883 interno · :1884 público   │
                            └────────────────┬──────────────────┘
                     subscribe               │               subscribe
           ┌─────────────────────────────────┤─────────────────────────────────┐
           ▼                                 ▼                                 ▼
┌─────────────────────┐        ┌───────────────────────────┐      ┌──────────────────────┐
│ App mobile (operador)│        │  Node-RED — Flow 1        │      │ App mobile / painéis  │
│ IoT MQTT Panel        │        │  Persistência + resumo    │      │ (estacionamento/*)    │
│ resumo / alerta / logs│        └─────────────┬──────────────┘      └──────────────────────┘
└──────────────────────┘                      │ SQL
                                               ▼
                            ┌─────────────────────────────────┐
                            │  PostgreSQL 15 (smartpark_postgres)│
                            │  vagas · historico_ocupacao       │
                            └────────────────┬──────────────────┘
                                             │ SELECT/UPDATE
                                             ▼
                            ┌─────────────────────────────────┐
                            │  Node-RED — Flow 3 e 4            │
                            │  API REST (/api/*) + Portal (/)   │
                            └────────────────┬──────────────────┘
                                             │ proxy_pass (rede mcorecloud-network)
                                             ▼
                            ┌─────────────────────────────────┐
                            │  nginx-master (compartilhado)     │
                            │  TLS Let's Encrypt · HTTPS 443    │
                            └────────────────┬──────────────────┘
                                             ▼
                              https://smartpark-red.sytes.net
                              Portal do Cliente (High-Tech / dark neon)
```

Cada serviço roda em container isolado, com rede própria (`smartpark_internal`)
para a comunicação interna Mosquitto ↔ Node-RED ↔ PostgreSQL. Só o Node-RED
também entra na rede `mcorecloud-network`, que é a rede **já existente** do
`nginx-master` compartilhado da VPS — assim o projeto se conecta ao proxy
reverso central sem expor portas extras e sem interferir nos demais projetos
hospedados na mesma máquina.

## Stack técnica

| Componente | Versão em produção | Papel |
|---|---|---|
| Docker Engine + Docker Compose v2 | — | Orquestração de todos os serviços |
| Eclipse Mosquitto | 2.1.2 | Broker MQTT (publish/subscribe) |
| Node-RED | 5.0.4 (Node.js 24) | Middleware: persistência, API REST, simulador, portal |
| `node-red-contrib-postgresql` | 0.16.x | Nó de integração Node-RED ↔ PostgreSQL |
| PostgreSQL | 15.18 (Alpine) | Persistência relacional (vagas + histórico) |
| Nginx (`nginx-master`) | — | Proxy reverso + TLS, compartilhado com outros projetos da VPS |
| Certbot / Let's Encrypt | — | Certificado TLS automático para `smartpark-red.sytes.net` |
| ESP32 DevKit v1 | simulado no [Wokwi](https://wokwi.com/) | Sensoriamento das 15 vagas |
| HTML5 + CSS3 + JavaScript (vanilla) | — | Portal do Cliente (sem frameworks/CDNs) |
| IoT MQTT Panel (Android) | — | Console do operador via MQTT |

## Estrutura do repositório

```
docker-compose.yml            # mosquitto + postgres + node-red
.env.example                  # copiar para .env com credenciais reais
mosquitto/config/              # mosquitto.conf
postgres/init/                 # 01_schema.sql, 02_seed.sql, 03_scenarios.sql (rodam no 1º boot)
nodered/
  ├─ settings.js               # editor protegido por login, portal estático, etc.
  ├─ package.json              # dependência node-red-contrib-postgresql
  └─ flows.template.json       # os 4 flows (credenciais são injetadas no deploy)
web/
  ├─ index.html                  # Landing page (/) — visão geral + botões Painel / Admin
  ├─ painel.html                  # Painel consolidado (/painel, alias /vagas)
  ├─ painel-simulador.html         # Painel do Cenário 1 — Node-RED (/painel/simulador)
  ├─ painel-wokwi.html             # Painel do Cenário 2 — Wokwi/ESP32 (/painel/wokwi)
  ├─ monitor.html                 # Monitor MQTT (/monitor) — MQTT sobre WebSocket ao vivo
  └─ vendor/mqtt.min.js            # cliente MQTT.js auto-hospedado (sem CDN)
esp32/
  ├─ sketch.ino                 # firmware ESP32 (Wokwi)
  ├─ diagram.json                # circuito Wokwi (15 botões + NeoPixel)
  ├─ wokwi.toml / libraries.txt
nginx/smartpark-red.conf        # vhost de referência para o nginx-master da VPS
scripts/deploy.sh               # renderiza flows.json a partir do .env e sobe os containers
docs/
  ├─ deploy.md                   # passo a passo completo de implantação na VPS
  └─ relatorio-academico.md      # guia para o relatório acadêmico do grupo
```

## Modelo de dados

```sql
CREATE TABLE vagas (
    id                VARCHAR(5)  PRIMARY KEY,   -- A01 .. C05
    andar             VARCHAR(15) NOT NULL,       -- "Andar 1", "Andar 2", "Andar 3"
    pino_esp32        INT         NOT NULL,       -- pino físico no ESP32
    preferencial      BOOLEAN     NOT NULL DEFAULT FALSE,
    ocupada           BOOLEAN     NOT NULL DEFAULT FALSE,
    contador_favorita INT         NOT NULL DEFAULT 0,
    atualizado_em     TIMESTAMP   NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE historico_ocupacao (
    id        SERIAL PRIMARY KEY,
    vaga_id   VARCHAR(5) NOT NULL REFERENCES vagas(id),
    ocupada   BOOLEAN    NOT NULL,
    timestamp TIMESTAMP  NOT NULL DEFAULT CURRENT_TIMESTAMP
);
```

`vagas` + `historico_ocupacao` são a visão **consolidada**, atualizada por
qualquer uma das duas vias de publicação e usada pelo Portal do Cliente e pela
API pública (`/api/vagas`). Para comprovar a independência das duas vias
(requisito do escopo acadêmico), cada uma também grava numa trilha isolada,
no mesmo banco `smartpark`, em tabelas próprias
(`postgres/init/03_scenarios.sql`):

```sql
-- Cenário 1: Node-RED (publisher de backup)
CREATE TABLE vagas_simulador (id, andar, pino_esp32, preferencial, ocupada, atualizado_em);
CREATE TABLE historico_simulador (id, vaga_id, ocupada, timestamp);

-- Cenário 2: Wokwi (ESP32, sensor físico simulado)
CREATE TABLE vagas_wokwi (id, andar, pino_esp32, preferencial, ocupada, atualizado_em);
CREATE TABLE historico_wokwi (id, vaga_id, ocupada, timestamp);
```

(mesma estrutura de `vagas`, sem `contador_favorita` — esse campo é específico
do Portal do Cliente).

As 15 vagas são semeadas automaticamente (`postgres/init/02_seed.sql`, e
replicadas para as tabelas por cenário em `03_scenarios.sql`) com o
mapeamento de pinos usado no firmware:

| Vaga | Andar | Pino ESP32 | Preferencial |
|---|---|---|---|
| A01–A04 | Andar 1 | 13, 12, 14, 27 | não |
| **A05** | Andar 1 | 26 | **sim** |
| B01–B04 | Andar 2 | 25, 33, 32, 35 | não |
| **B05** | Andar 2 | 34 | **sim** |
| C01–C04 | Andar 3 | 23, 22, 21, 19 | não |
| **C05** | Andar 3 | 18 | **sim** |

## Fluxos do Node-RED

O middleware (`nodered/flows.template.json`) organiza a lógica em 4 flows:

1. **Processador IoT & Persistência** — `mqtt in` no tópico
   `estacionamento/vagas/status` → parse JSON → dois ramos em paralelo:
   - **Consolidado**: `UPDATE vagas` + `INSERT historico_ocupacao` →
     recalcula o resumo geral → publica `estacionamento/resumo` (retido) e,
     se lotado ou vaga PCD ocupada, `estacionamento/alerta`.
   - **Por cenário**: lê o campo `origem` do payload (`"wokwi"` ou
     `"simulador"`) e grava na tabela isolada correspondente
     (`vagas_wokwi`/`vagas_simulador` + histórico), recalcula o resumo
     **daquele cenário** e publica em `estacionamento/<cenário>/resumo`,
     `estacionamento/<cenário>/alerta` e um tópico retido por vaga
     (`estacionamento/<cenário>/vagas/<ID>/status`).
2. **Simulador de backup** — `inject` a cada 15s → sorteia uma vaga → consulta
   o estado atual no Postgres → inverte e publica no mesmo tópico do Wokwi,
   marcando `"origem":"simulador"` no payload (o broker entrega de volta ao
   próprio Node-RED, reaproveitando o Flow 1).
3. **API HTTP** — `GET /api/vagas` (consolidado), `GET /api/vagas/simulador`
   e `GET /api/vagas/wokwi` (por cenário), e `POST /api/favorita`
   (`{"vaga_id":"A01"}`, incrementa o contador).
4. **Servidor do portal** — serve a landing (`/`, estática), o painel
   consolidado (`GET /painel`, alias `GET /vagas`), os painéis por cenário
   (`GET /painel/simulador`, `GET /painel/wokwi`) e o monitor MQTT
   (`GET /monitor`).

O ESP32 identifica sua origem publicando `"origem":"wokwi"` no payload (ver
`esp32/sketch.ino`); o simulador do Node-RED publica `"origem":"simulador"`.
Mensagens sem esse campo (compatibilidade com versões antigas do firmware)
são tratadas como `"wokwi"` por padrão.

## Tópicos MQTT

| Tópico | Publicado por | QoS | Retain | Exemplo de payload |
|---|---|---|---|---|
| `estacionamento/vagas/status` | ESP32 / simulador Node-RED | 0–1 | não | `{"vaga_id":"A01","ocupada":true,"preferencial":false,"andar":"Andar 1"}` |
| `estacionamento/resumo` | Node-RED | 1 | **sim** | `{"livres":11,"ocupadas":4,"total":15,"taxa_ocupacao":26.7,"por_andar":{...}}` |
| `estacionamento/alerta` | Node-RED | 1 | não | `{"nivel":"atencao","mensagem":"Alerta: Vaga PCD ocupada sem autorização!","vagas":["A05"]}` |
| `estacionamento/vagas/<ID>/status` | Node-RED | 1 | **sim** | payload simples `LIVRE` / `OCUPADA` / `OCUPADA_PCD` — um tópico por vaga (ex: `estacionamento/vagas/A01/status`), pensado para widgets/cards individuais em apps como IoT MQTT Panel |
| `estacionamento/<cenário>/resumo` | Node-RED | 1 | **sim** | idem `estacionamento/resumo`, mas só com dados daquele cenário. `<cenário>` = `simulador` ou `wokwi` |
| `estacionamento/<cenário>/alerta` | Node-RED | 1 | não | idem `estacionamento/alerta`, isolado por cenário |
| `estacionamento/<cenário>/vagas/<ID>/status` | Node-RED | 1 | **sim** | `LIVRE`/`OCUPADA`/`OCUPADA_PCD` isolado por cenário — cards separados por fonte no IoT MQTT Panel |

O retain em `estacionamento/resumo` garante que qualquer assinante novo
(app mobile, painel) recebe o último estado consolidado imediatamente ao se
conectar, sem esperar o próximo evento. O mesmo vale para os tópicos por
cenário.

## API REST

| Método | Rota | Descrição |
|---|---|---|
| `GET` | `/api/vagas` | Lista as 15 vagas (visão consolidada) |
| `GET` | `/api/vagas/simulador` | Vagas do Cenário 1 (Node-RED) |
| `GET` | `/api/vagas/wokwi` | Vagas do Cenário 2 (ESP32/Wokwi) |
| `POST` | `/api/favorita` | `{"vaga_id":"A01"}` → incrementa `contador_favorita` |
| `GET` | `/` | Landing page do projeto |
| `GET` | `/painel` (alias `/vagas`) | Painel de vagas consolidado, em tempo real |
| `GET` | `/painel/simulador` | Painel do Cenário 1 (Node-RED) |
| `GET` | `/painel/wokwi` | Painel do Cenário 2 (Wokwi/ESP32) |
| `GET` | `/monitor` | Monitor MQTT ao vivo (WebSocket) |

## Portal web do cliente

Cinco páginas HTML/CSS/JS vanilla (sem frameworks), tema **dark neon
high-tech**, servidas pelo próprio Node-RED (Flow 4):

- **`/` — Landing**: visão geral do projeto (a mesma informação deste README,
  resumida), arquitetura, stack técnica, tópicos MQTT, e os 2 botões de acesso
  rápido: **Painel de Vagas** e **Admin Node-RED**.
- **`/painel` (alias `/vagas`) — Painel Consolidado**: bento grid com vagas
  livres/ocupadas/taxa de ocupação, filtros por andar e por PCD, grade das 15
  vagas coloridas por status, favoritar vaga (`localStorage` +
  `POST /api/favorita`), atualização automática a cada 2s via `fetch` em
  `/api/vagas`.
- **`/painel/simulador` — Cenário 1**: mesmo estilo visual, mas mostrando
  apenas os dados que vieram do publisher de backup do Node-RED
  (`/api/vagas/simulador`), isolados na tabela `vagas_simulador`.
- **`/painel/wokwi` — Cenário 2**: idem, só com os dados publicados pelo
  ESP32 no Wokwi (`/api/vagas/wokwi`, tabela `vagas_wokwi`).
- **`/monitor` — Monitor MQTT**: conecta direto no broker via **MQTT sobre
  WebSocket** (`wss://.../mqtt-ws`, proxiado pelo Nginx até o listener 9001 do
  Mosquitto) usando o cliente `mqtt.js` auto-hospedado em `web/vendor/`. Assina
  `estacionamento/#` e atualiza vagas, contadores e um log de mensagens em
  tempo real — a versão web do que o app **myMQTT** mostra no celular. A
  própria página traz a tabela de host/porta/tópicos para quem preferir
  configurar o myMQTT/MQTT Dash em vez de usar o navegador.

Os painéis por cenário existem para comprovar, de forma visual e
independente, que as duas vias de publicação (sensor físico simulado no
Wokwi e publisher de backup no Node-RED) realmente operam — e persistem —
de forma isolada uma da outra, sem que uma mascare falhas da outra.

## Firmware ESP32 / Wokwi

`esp32/sketch.ino` — C++ com `WiFi.h` (rede `Wokwi-GUEST`), `PubSubClient.h`
(MQTT) e `ArduinoJson.h`:

- 15 botões digitais (um por vaga, `INPUT_PULLUP`) com debounce por software;
- varredura completa no `setup()` para sincronizar a VPS assim que o
  dispositivo liga;
- a cada mudança de estado, publica em `estacionamento/vagas/status`;
- feedback visual em **1 fita NeoPixel (WS2812) de 15 LEDs**, um pixel por
  vaga — verde/vermelho/azul conforme o status. Essa escolha substitui 45
  LEDs discretos (3 por vaga), inviáveis no orçamento de pinos de um único
  ESP32, sem abrir mão do feedback individual por vaga.

Projeto Wokwi completo em `esp32/` (`sketch.ino`, `diagram.json`,
`libraries.txt`, `wokwi.toml`).

## App mobile (operador)

Configuração recomendada para **IoT MQTT Panel** / MQTT Dash:

| Campo | Valor |
|---|---|
| Host | `smartpark-red.sytes.net` |
| Porta | `1884` |
| TLS | desligado (MQTT puro) |

Assinaturas: `estacionamento/resumo` (contadores), `estacionamento/alerta`
(notificações), `estacionamento/vagas/status` (log de eventos). Para
acompanhar só um cenário, assine `estacionamento/simulador/#` (Node-RED) ou
`estacionamento/wokwi/#` (ESP32) em vez do tópico consolidado. Passo a passo
completo (incluindo cards coloridos por vaga) em
[`docs/deploy.md`](docs/deploy.md).

## Segurança

- **Portal e API**: servidos via HTTPS (Let's Encrypt) através do
  `nginx-master` compartilhado da VPS.
- **Editor de flows do Node-RED**: `https://smartpark-red.sytes.net/admin-red`,
  protegido por login (`adminAuth` com senha em bcrypt) — acessível de
  qualquer navegador, sem precisar de túnel SSH ou ambiente gráfico no
  servidor.
- **Broker MQTT**: aceita conexões anônimas (`allow_anonymous true`),
  decisão deliberada para simplificar a conexão de app mobile e ESP32 num
  cenário acadêmico de demonstração — ver [Limitações](#limitações-conhecidas-e-trabalhos-futuros).
- **Segredos**: senhas do Postgres, `credentialSecret` do Node-RED e hash da
  senha do editor ficam em `.env` (fora do Git); o repositório só versiona
  `.env.example` com placeholders.
- **Isolamento de rede**: Postgres e Mosquitto só são alcançáveis dentro da
  rede interna do projeto (exceto a porta MQTT pública 1884, necessária para
  ESP32/app); nenhum dado sensível trafega por ali.

## Deploy

Guia completo, com todos os comandos prontos para colar na VPS (incluindo
emissão do certificado SSL e configuração do vhost do Nginx compartilhado),
em **[`docs/deploy.md`](docs/deploy.md)**.

Resumo rápido (rodando dentro da pasta do projeto, já com Docker instalado):

```bash
git clone https://github.com/jorgyvanlima/smartpark-red.git
cd smartpark-red
cp .env.example .env   # edite com senhas fortes
./scripts/deploy.sh
```

## Limitações conhecidas e trabalhos futuros

- **MQTT sem TLS**: a porta pública 1884 trafega MQTT em texto plano. Para
  produção real, o próximo passo seria migrar para MQTTS (porta 8883) com
  certificado próprio para o broker.
- **Broker anônimo**: não há autenticação nem ACL por tópico no Mosquitto.
  O `mosquitto.conf` já deixa comentado como habilitar usuário/senha
  (`mosquitto_passwd`) quando isso deixar de ser aceitável.
- **Vaga PCD sem verificação de autorização real**: o alerta de
  "vaga PCD ocupada" é disparado por qualquer ocupação da vaga preferencial,
  já que não há leitor de credencial (cartão/QR) no protótipo.

## Equipe

Projeto desenvolvido para a disciplina **Tecnologias de Comunicação em IoT**
— Curso de Especialização em Sistemas de Segurança Integrada da Informação e
Cibersegurança, **UFPA** (Belém-PA, 2026).

**Grupo D**<p>
Arienilce Sacramento Gonçalves <p>· Clisciano Nascimento Souza ·
Flávio Alexandre Souza Nunes  <p>·
**Jorgyvan Braga Lima** <p> ·
Józimo Azevedo Botelho <p>· Osvaldo José Rodrigues Neves <p>· Thiago Bitar Cruz<p> ·
Wallace Pablo Rocha da Cruz <p>· Vinícius Antônio de Paula Valente <p>· Josiane Moraes

Orientação:<p> Dr. Fabrício José Brito Barros · <p>Me. Joel Alison Ribeiro Carvalho
