# SmartPark-RED

Estacionamento inteligente de 3 andares (15 vagas, `A01`–`C05`, com `A05`/`B05`/`C05`
preferenciais), 100% Docker, publicado em `https://smartpark-red.sytes.net`.

## Arquitetura

```
Wokwi (ESP32) ----\
                    \                         +--------------+
Node-RED (simulador) +--> MQTT (Mosquitto) -->|  Node-RED    |--> PostgreSQL
                    /       :1884              |  (middleware)|
App Mobile MQTT ---/                           +--------------+
                                                       |
                                                 HTTP API (/api/*)
                                                       |
                                              Nginx (TLS) --> Portal Web (High-Tech)
```

- **Mosquitto** (`smartpark_mosquitto`): broker MQTT, porta pública `1884` (host) → `1883` (container).
- **PostgreSQL 15** (`smartpark_postgres`): banco `vagas` + `historico_ocupacao`, somente na rede interna.
- **Node-RED** (`smartpark_nodered`): 4 flows (persistência IoT, simulador backend, API HTTP, portal web),
  também serve o Portal do Cliente estático e fica atrás do Nginx compartilhado da VPS.
- **Nginx** (`nginx-master`, já existente na VPS): faz o reverse proxy com TLS (Let's Encrypt) para este e
  outros projetos, sem conflito de portas ou redes.

Ver [`docs/deploy.md`](docs/deploy.md) para o passo a passo completo de implantação e
[`docs/relatorio-academico.md`](docs/relatorio-academico.md) para o guia do relatório.

## Estrutura

```
docker-compose.yml         # mosquitto + postgres + node-red
.env.example                # copiar para .env com credenciais reais
mosquitto/config/           # mosquitto.conf
postgres/init/               # schema + seed (executado automaticamente no 1º boot)
nodered/                     # settings.js, package.json, flows.template.json
web/index.html                # Portal do Cliente (High-Tech, dark neon)
esp32/                        # sketch.ino, diagram.json, wokwi.toml (projeto Wokwi)
nginx/smartpark-red.conf      # vhost de referência para o nginx-master da VPS
scripts/deploy.sh             # renderiza flows.json a partir do .env e sobe os containers
docs/                         # deploy.md, relatorio-academico.md
```

## Deploy rápido (na VPS, dentro da pasta do projeto)

```bash
cp .env.example .env   # edite com senhas fortes
./scripts/deploy.sh
```

Depois disso, configure o vhost do Nginx e o certificado SSL — ver `docs/deploy.md`.

## Tópicos MQTT

| Tópico                              | Direção            | QoS | Retain | Descrição                                   |
|--------------------------------------|---------------------|-----|--------|-----------------------------------------------|
| `estacionamento/vagas/status`        | ESP32/simulador → NR | 0-1 | não    | Evento de mudança de ocupação de 1 vaga       |
| `estacionamento/resumo`              | NR → assinantes      | 1   | sim    | Contadores consolidados (livres/ocupadas/%)   |
| `estacionamento/alerta`              | NR → assinantes      | 1   | não    | `LOTADO` ou vaga PCD ocupada                  |

## API HTTP

| Método | Rota            | Descrição                                   |
|--------|------------------|-----------------------------------------------|
| GET    | `/api/vagas`     | Lista as 15 vagas com status atual            |
| POST   | `/api/favorita`  | `{"vaga_id":"A01"}` — incrementa o contador   |
| GET    | `/vagas` ou `/`  | Portal Web do cliente                         |
