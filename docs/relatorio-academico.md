# Guia para o Relatório Acadêmico (Modelo_Relatorio_MQTT_IoT.docx)

Como preencher cada seção do relatório usando os recursos implementados no
SmartPark-RED.

## Introdução

Destaque o problema real de mobilidade urbana: encontrar vaga em estacionamentos
de múltiplos andares consome tempo e combustível, e a telemetria IoT (sensores +
MQTT + dashboards) permite informar em tempo real onde há vaga livre, reduzindo
circulação desnecessária — um caso concreto de Smart City. Cite que o projeto usa
15 vagas reais (3 andares, vagas preferenciais PCD/idoso/elétrico) simuladas via
Wokwi (ESP32), com dupla via de publicação (sensor físico + simulador de backend)
para garantir robustez de demonstração mesmo sem hardware físico presente.

## Arquitetura da Solução

Use o diagrama do [`README.md`](../README.md#arquitetura) como base e descreva o
fluxo completo:

```
Wokwi (ESP32, sensor)  ---\
                            +--> Mosquitto (broker MQTT, :1884) --> Node-RED (middleware)
Node-RED (simulador)   ---/                                              |
                                                                    PostgreSQL 15
                                                                          |
App Mobile (IoT MQTT Panel) <--- assina MQTT                    HTTP API /api/*
                                                                          |
                                                          Nginx (TLS) --> Portal Web
```

Explique que o **broker** é o ponto único de desacoplamento: o ESP32 (ou o
simulador do Node-RED) publica eventos de ocupação, e qualquer número de
assinantes (o próprio Node-RED para persistência, o app do operador, painéis de
monitoramento) recebe em tempo real sem acoplamento direto ao sensor.

## Recursos de Hardware e Software

| Componente        | Versão / Detalhe                                  |
|--------------------|-----------------------------------------------------|
| Microcontrolador    | ESP32 DevKit v1 (simulado no Wokwi)                 |
| Broker MQTT          | Eclipse Mosquitto 2.x (Docker, porta 1884)          |
| Middleware            | Node-RED (imagem oficial `nodered/node-red:latest`)  |
| Banco de dados         | PostgreSQL 15 (Alpine)                               |
| Orquestração             | Docker + Docker Compose v2                            |
| Proxy/TLS                 | Nginx + Let's Encrypt (Certbot)                        |
| App de monitoramento        | IoT MQTT Panel (Android) ou MQTT Dash                   |
| URL do projeto Wokwi          | (cole aqui o link do seu projeto após publicá-lo)         |
| Domínio de produção             | `https://smartpark-red.sytes.net`                            |

## Mapeamento de Tópicos

| Tópico                             | Publicado por          | Assinado por                  | QoS | Retain | Exemplo de payload |
|--------------------------------------|--------------------------|----------------------------------|-----|--------|-----------------------|
| `estacionamento/vagas/status`        | ESP32 / simulador Node-RED | Node-RED (persistência), app mobile | 0-1 | não    | `{"vaga_id":"A01","ocupada":true,"preferencial":false,"andar":"Andar 1"}` |
| `estacionamento/resumo`              | Node-RED                  | App mobile, painéis                | 1   | sim    | `{"livres":8,"ocupadas":7,"total":15,"taxa_ocupacao":46.7,"por_andar":{"Andar 1":{"total":5,"ocupadas":2,"livres":3}}}` |
| `estacionamento/alerta`              | Node-RED                  | App mobile                          | 1   | não    | `{"nivel":"critico","mensagem":"LOTADO","timestamp":"2026-08-13T12:00:00Z"}` |

## Resultados e Análise

Roteiro sugerido de testes para o relatório:

1. **QoS**: publique com QoS 0 e QoS 1 em `estacionamento/vagas/status` e compare
   a garantia de entrega desligando/religando o Wi-Fi do ESP32 simulado no Wokwi —
   documente se a mensagem foi perdida (QoS 0) ou reentregue (QoS 1).
2. **Retain**: reinicie o app mobile e observe que `estacionamento/resumo`
   chega imediatamente ao assinar (mensagem retida), enquanto
   `estacionamento/vagas/status` só chega em eventos novos (sem retain).
3. **Latência sensor → app**: cronometre o tempo entre o clique no botão do Wokwi
   e a atualização no app mobile / portal web (meça 5-10 amostras e calcule a
   média).
4. **Latência simulador → portal**: com o Wokwi desligado, deixe o Flow 2 do
   Node-RED (simulador de 15 em 15s) rodando e confirme que o portal web
   (polling de 2s) continua refletindo mudanças de estado — evidencia a
   independência da via de backup.
5. **Persistência histórica**: consulte `historico_ocupacao` no PostgreSQL e
   mostre que cada mudança de estado gerou um registro (`SELECT * FROM
   historico_ocupacao ORDER BY timestamp DESC LIMIT 20;`).

Inclua prints do Wokwi, do app mobile (IoT MQTT Panel) e do Portal Web
lado a lado para cada teste, e feche com uma conclusão sobre a viabilidade da
arquitetura dupla via (sensor real + simulador) para ambientes acadêmicos.
