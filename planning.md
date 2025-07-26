Projeto ESP32 para controle de carro/drone/aviação remoto.

- Conexão por Wifi ou Bluetooth
- Update do firmware por OTA (Over the Air)
- View em HTML hospedada no proprio ESP32
- Comandos via Websocket

Projeto Html:
    Folder: /dev_html
    - Todo HTML deve ficar no único arquivo "index.html"
    - Todo javascript deve ficar no único arquivo "script.js"
    - Todo CSS deve ficar no único arquivo "style.css"
    - Maxima otimização do JS e CSS

Project ESP32:
    Folder: /main
    - Arquivo /main/wwwroot/index.html é gerado pelo script "\dev_html> node run prod"
    - O OTA vai ser enviado pelo HTML

Views:
    Main:
        Divisão por colunas
            Primeira coluna um botão de controle de velocidade do lado esquerdo
            Resto das colunas
                Divisão por linhas
                    Linha do topo sequencia de botões
                        Primeiro botão abre tela de configuração
                        Segundo botão de uso geral
                    Linha alinha abaixo
                        Botão de controle de direção alinhado a direita

    Configuration:
        Card - Configuration
        No body um tab com itens a esquerda, itens:
            General
            Wifi
            OTA
        Tab General:
            Botão de escolha da conexão, Bluetooth ou WIFI
        Tab Wifi:
            Campo para entrar o nome do WIFI
            Campo para entrar a senha do WIFI
            Botão para Gravar
        Tab OTA:
            Botão para selecionar o arquivo .bin para fazer upload
            Botão para Atualizar


