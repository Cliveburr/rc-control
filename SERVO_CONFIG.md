# Configuração do Servo GH-S37D

## Conexões GPIO

### Servo GH-S37D (3.7g Mini Micro Servo)
- **GPIO4** - Sinal de controle PWM do servo
- **3.3V** - Alimentação VCC (compatível com 1S LiPo 3.7V)
- **GND** - Terra comum

### Esquema de Conexão
```
ESP32        GH-S37D Servo
GPIO4   -->  Sinal (amarelo/branco)
3.3V    -->  VCC (vermelho)
GND     -->  GND (marrom/preto)
```

## Especificações Técnicas

### Servo GH-S37D
- **Tensão**: 3.0V - 4.2V (ideal para 1S LiPo)
- **Peso**: 3.7g
- **Torque**: ~0.8kg.cm
- **Velocidade**: ~0.10s/60°
- **Controle**: PWM padrão (50Hz, 1-2ms pulse width)

### Configuração PWM
- **Frequência**: 50Hz (período de 20ms)
- **Largura de pulso**:
  - 1.0ms = -90° (máximo à esquerda)
  - 1.5ms = 0° (centro)
  - 2.0ms = +90° (máximo à direita)
- **Resolução**: 13 bits (8192 níveis)
- **Timer LEDC**: TIMER_1
- **Canal LEDC**: CHANNEL_1

## Comando de Controle

### Via WebSocket
```json
{
  "type": "wheels",
  "value": 0
}
```

**Valores do comando:**
- `-100` = Máximo à esquerda (-90°)
- `0` = Centro (0°)
- `+100` = Máximo à direita (+90°)

### Função de Controle
```c
esp_err_t servo_control_set_position(int position);
```

**Parâmetros:**
- `position`: Valor entre -100 e +100
- **Retorno**: `ESP_OK` se bem-sucedido

## Instalação Física

### Montagem
1. Fixar o servo na estrutura do chassi usando parafusos M2
2. Conectar o braço servo (servo horn) ao sistema de direção
3. Ajustar linkagem para que posição central corresponda a rodas retas

### Linkagem de Direção
- Use barras de ligação rígidas (pushrods) de 0.8-1.0mm
- Terminais esféricos (ball links) M2 para conexões articuladas
- Comprimento da linkagem deve permitir curso completo sem travamento

### Alimentação
- O servo pode ser alimentado diretamente da bateria 1S LiPo (3.7V)
- Consumo típico: 50-150mA em operação normal
- Pico de corrente: até 500mA durante movimento rápido

## Calibração

### Valores de Trim
Se necessário ajustar o centro mecânico:
1. Modificar `SERVO_CENTER_PULSE_WIDTH` em `servo_control.h`
2. Valores típicos: 1450-1550μs dependendo do servo

### Limitação de Curso
Para evitar danos mecânicos:
1. Ajustar `SERVO_MIN_PULSE_WIDTH` e `SERVO_MAX_PULSE_WIDTH`
2. Testar movimento completo antes de finalizar montagem

## Troubleshooting

### Servo não responde
- Verificar conexões GPIO4, 3.3V e GND
- Confirmar tensão de alimentação (3.0-4.2V)
- Verificar logs no monitor serial

### Movimento irregular
- Verificar interferência eletromagnética
- Adicionar capacitor de filtro (100μF) na alimentação
- Afastar fios do servo dos fios do motor principal

### Consumo excessivo
- Verificar se linkagem não está travando
- Reduzir carga mecânica no servo
- Verificar alinhamento das peças móveis