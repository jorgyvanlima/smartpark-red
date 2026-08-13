#!/usr/bin/env bash
# SmartPark-RED :: deploy/redeploy na VPS
# Uso: ./scripts/deploy.sh
set -euo pipefail
cd "$(dirname "$0")/.."

if [ ! -f .env ]; then
    echo "Arquivo .env não encontrado. Copie .env.example para .env e preencha os valores." >&2
    exit 1
fi
set -a
source .env
set +a

mkdir -p nodered/data mosquitto/data mosquitto/log

# O container node-red roda como uid 1000; a pasta precisa pertencer a ele
# para o Node-RED conseguir escrever (node_modules, flows_cred.json etc.),
# mas para (re)gerar o flows.json aqui a gente precisa poder escrever nela.
sudo chown -R "$(id -u):$(id -g)" nodered/data

# Renderiza o flows.template.json com as credenciais do .env (arquivo final
# fica em nodered/data/, fora do git)
sed \
    -e "s/__PG_HOST__/smartpark_postgres/g" \
    -e "s/__PG_PORT__/5432/g" \
    -e "s/__PG_DATABASE__/${PG_DATABASE}/g" \
    -e "s/__PG_USER__/${PG_USER}/g" \
    -e "s/__PG_PASSWORD__/${PG_PASSWORD}/g" \
    nodered/flows.template.json > nodered/data/flows.json

sudo chown -R 1000:1000 nodered/data

docker compose pull
docker compose up -d --build

echo "Deploy concluído. Containers:"
docker compose ps
