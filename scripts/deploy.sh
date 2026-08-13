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

# Renderiza o flows.template.json com as credenciais do .env (arquivo final
# fica em nodered/data/, fora do git)
sed \
    -e "s/__PG_HOST__/smartpark_postgres/g" \
    -e "s/__PG_PORT__/5432/g" \
    -e "s/__PG_DATABASE__/${PG_DATABASE}/g" \
    -e "s/__PG_USER__/${PG_USER}/g" \
    -e "s/__PG_PASSWORD__/${PG_PASSWORD}/g" \
    nodered/flows.template.json > nodered/data/flows.json

docker compose pull
docker compose up -d --build

echo "Deploy concluído. Containers:"
docker compose ps
