-- SmartPark-RED :: Cenários de telemetria separados (mesmo banco, tabelas
-- dedicadas) para comprovar a independência da dupla via de publicação:
--   Cenário 1 — Node-RED (Publisher de Backup / simulador temporizado)
--   Cenário 2 — Wokwi (ESP32, sensor físico simulado)
--
-- A tabela "vagas" + "historico_ocupacao" (01_schema.sql) continuam sendo a
-- visão CONSOLIDADA usada pelo Portal do Cliente e pela API pública; estas
-- tabelas aqui são a trilha isolada de cada fonte, usada pelos painéis de
-- monitoramento /painel/simulador e /painel/wokwi.

CREATE TABLE IF NOT EXISTS vagas_simulador (
    id             VARCHAR(5)  PRIMARY KEY,
    andar          VARCHAR(15) NOT NULL,
    pino_esp32     INT         NOT NULL,
    preferencial   BOOLEAN     NOT NULL DEFAULT FALSE,
    ocupada        BOOLEAN     NOT NULL DEFAULT FALSE,
    atualizado_em  TIMESTAMP   NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS historico_simulador (
    id        SERIAL PRIMARY KEY,
    vaga_id   VARCHAR(5) NOT NULL REFERENCES vagas_simulador(id),
    ocupada   BOOLEAN    NOT NULL,
    timestamp TIMESTAMP  NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS vagas_wokwi (
    id             VARCHAR(5)  PRIMARY KEY,
    andar          VARCHAR(15) NOT NULL,
    pino_esp32     INT         NOT NULL,
    preferencial   BOOLEAN     NOT NULL DEFAULT FALSE,
    ocupada        BOOLEAN     NOT NULL DEFAULT FALSE,
    atualizado_em  TIMESTAMP   NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS historico_wokwi (
    id        SERIAL PRIMARY KEY,
    vaga_id   VARCHAR(5) NOT NULL REFERENCES vagas_wokwi(id),
    ocupada   BOOLEAN    NOT NULL,
    timestamp TIMESTAMP  NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_historico_simulador_vaga_id ON historico_simulador (vaga_id);
CREATE INDEX IF NOT EXISTS idx_historico_wokwi_vaga_id ON historico_wokwi (vaga_id);

INSERT INTO vagas_simulador (id, andar, pino_esp32, preferencial, ocupada)
SELECT id, andar, pino_esp32, preferencial, FALSE FROM vagas
ON CONFLICT (id) DO NOTHING;

INSERT INTO vagas_wokwi (id, andar, pino_esp32, preferencial, ocupada)
SELECT id, andar, pino_esp32, preferencial, FALSE FROM vagas
ON CONFLICT (id) DO NOTHING;
