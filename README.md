
# RC Control - Projeto ESP32 para Controle Remoto

## Funcionalidades

- ✅ Conexão WiFi (AP mode ou Station mode)
- ✅ Interface Web hospedada no ESP32
- ✅ Transmissão de vídeo via streaming
- ✅ **Sistema OTA (Over-The-Air) para atualizações de firmware**
- ✅ Configuração via interface web
- ✅ Armazenamento de configurações em memória não volátil

## Funcionalidades OTA Implementadas

### Configuração de Partições
- Partições configuradas para suportar OTA com rollback automático
- Duas partições de aplicação (ota_0 e ota_1) para atualizações seguras
- Partição otadata para controle do sistema OTA

### Interface Web para OTA
- **Aba OTA na configuração:** Acesse através do botão de configuração
- **Status do sistema:** Mostra partição atual em execução
- **Upload de firmware:** Envio de arquivos .bin através da interface web
- **Barra de progresso:** Acompanhamento visual do upload
- **Validação automática:** Verificação de integridade do firmware

### Endpoints HTTP para OTA
- `GET /ota/status` - Retorna informações sobre o estado atual das partições
- `POST /ota/upload` - Recebe e instala novo firmware

### Segurança e Rollback
- Rollback automático habilitado em caso de falha na inicialização
- Validação de firmware antes da instalação
- Reinicialização automática após atualização bem-sucedida

## Como Usar o Sistema OTA

1. **Acesse a interface web** do ESP32 através do navegador
2. **Clique no botão de configuração** (primeiro botão superior)
3. **Selecione a aba "OTA"** no menu lateral
4. **Verifique o status atual** do sistema
5. **Selecione o arquivo .bin** do novo firmware
6. **Clique em "Atualizar Firmware"** para iniciar o processo
7. **Aguarde o upload** (barra de progresso será exibida)
8. **O dispositivo reiniciará automaticamente** após a atualização

## Estrutura do Projeto

### Frontend (`/dev_html`)
- `index.html` - Interface principal com controles e configuração
- `script.js` - Lógica JavaScript incluindo funcionalidades OTA
- `style.css` - Estilos da interface
- `prod.js` - Script para gerar versão otimizada

### Backend ESP32 (`/main`)
- `src/main.c` - Inicialização do sistema e OTA
- `src/ota.c` - Implementação completa do sistema OTA
- `src/http_server.c` - Servidor web com endpoints OTA
- `src/config.c` - Gerenciamento de configurações
- `src/net.c` - Configuração de rede WiFi
- `inc/ota.h` - Definições do sistema OTA

## Compilação e Flash

```bash
# Configurar ambiente ESP-IDF
idf.py build

# Flash inicial (primeira vez)
idf.py flash

# Atualizações subsequentes podem ser feitas via OTA através da interface web
```

## Tarefas Completadas

- ✅ Salvar dados em memória não volátil
- ✅ Configurar WiFi (AP e Station mode)
- ✅ Servir HTML e WebSocket
- ✅ Transmitir vídeo via HTTP streaming
- ✅ **Sistema OTA completo com interface web**
- ✅ **Configuração de partições para OTA**
- ✅ **Upload de firmware via browser**
- ✅ **Rollback automático em caso de falha**

## Próximos Passos

- Melhorar qualidade da imagem
- Implementar controles de movimento
- Adicionar autenticação para OTA
- Logs detalhados do processo OTA
