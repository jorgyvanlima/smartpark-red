-- SmartPark-RED :: Schema PostgreSQL
-- Executado automaticamente pelo container postgres no primeiro start
-- (montado em /docker-entrypoint-initdb.d)

CREATE TABLE IF NOT EXISTS vagas (
    id               VARCHAR(5)  PRIMARY KEY,
    andar            VARCHAR(15) NOT NULL,
    pino_esp32       INT         NOT NULL,
    preferencial     BOOLEAN     NOT NULL DEFAULT FALSE,
    ocupada          BOOLEAN     NOT NULL DEFAULT FALSE,
    contador_favorita INT        NOT NULL DEFAULT 0,
    atualizado_em    TIMESTAMP   NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS historico_ocupacao (
    id        SERIAL PRIMARY KEY,
    vaga_id   VARCHAR(5) NOT NULL REFERENCES vagas(id),
    ocupada   BOOLEAN    NOT NULL,
    timestamp TIMESTAMP  NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_historico_vaga_id ON historico_ocupacao (vaga_id);
CREATE INDEX IF NOT EXISTS idx_historico_timestamp ON historico_ocupacao (timestamp);
