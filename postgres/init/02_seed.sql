-- SmartPark-RED :: Seed das 15 vagas (A01-C05)
-- Mapeamento de pinos físicos do ESP32 (Wokwi) — ver esp32/sketch.ino e esp32/diagram.json
-- Botões: 15 pinos digitais individuais (um por vaga)
-- LEDs: 1 fita NeoPixel (WS2812, 15 LEDs) no pino GPIO 4 — 1 LED endereçável por vaga
--       (substitui 45 LEDs discretos, inviável no orçamento de pinos do ESP32;
--        cada pixel assume verde/vermelho/azul conforme o status da vaga)

INSERT INTO vagas (id, andar, pino_esp32, preferencial, ocupada) VALUES
    ('A01', 'Andar 1', 13, FALSE, FALSE),
    ('A02', 'Andar 1', 12, FALSE, FALSE),
    ('A03', 'Andar 1', 14, FALSE, FALSE),
    ('A04', 'Andar 1', 27, FALSE, FALSE),
    ('A05', 'Andar 1', 26, TRUE,  FALSE),

    ('B01', 'Andar 2', 25, FALSE, FALSE),
    ('B02', 'Andar 2', 33, FALSE, FALSE),
    ('B03', 'Andar 2', 32, FALSE, FALSE),
    ('B04', 'Andar 2', 35, FALSE, FALSE),
    ('B05', 'Andar 2', 34, TRUE,  FALSE),

    ('C01', 'Andar 3', 23, FALSE, FALSE),
    ('C02', 'Andar 3', 22, FALSE, FALSE),
    ('C03', 'Andar 3', 21, FALSE, FALSE),
    ('C04', 'Andar 3', 19, FALSE, FALSE),
    ('C05', 'Andar 3', 18, TRUE,  FALSE)
ON CONFLICT (id) DO NOTHING;
