
Versao v1 de chassis controlado por radio tradicional, 15 cm de comprimento, sem amortecedor, com o maximo de partes imprimiveis e usando componentes RC convencionais.

## Premissas da v1

- Comprimento alvo do chassi: 150 mm
- Arquitetura: full RC tradicional
- Alimentacao: LiPo 2S
- Direcao por servo padrao micro
- Tracao traseira simples
- Maximo de partes impressas, usando metal apenas onde faz sentido mecanico

## Arquitetura recomendada

### Conjunto base
- Radio: FlySky FS-I6X
- Receptor: micro receiver compativel com AFHDS 2A, 3 a 6 canais
- Motor: brushed tamanho 130, tensao 6 V a 7.4 V, eixo 2.0 mm
- ESC: brushed automotivo 2S com reverso e BEC 5 V integrado
- Servo: micro servo 5 V, entre 5 g e 9 g, torque minimo de 1.5 kg.cm
- Bateria: LiPo 2S de 450 mAh a 850 mAh
- Rolamentos: MR52ZZ, 2 x 5 x 2.5 mm
- Eixo traseiro: aco 2 mm
- Parafusos: M2 como padrao principal, M3 nos pontos estruturais

## Levantamento de componentes

### 1. Componentes principais

#### Motor de tracao
- Tipo: brushed tamanho 130
- Tensao de trabalho: 6.0 V a 7.4 V
- Tensao maxima: 8.4 V
- Eixo: 2.0 mm
- Faixa de corrente do conjunto: usar ESC de 10 A a 20 A
- Termo de busca: 130 brushed motor 8.4V 12000rpm

- Decidido:
MABUCHI FC-130RA-10300 Small 130 Motor DC 5V-12V 6V 9V High Speed18000RPM Micro 20mm Electric Motor DIY RC Toy Car Boat Model
https://pt.aliexpress.com/item/1005009147508182.html?spm=a2g0o.productlist.main.26.5eea3453x4ZIOM&algo_pvid=644773f6-91c0-4f31-b929-8a52a4d48319&algo_exp_id=644773f6-91c0-4f31-b929-8a52a4d48319-25&pdp_ext_f=%7B%22order%22%3A%2290%22%2C%22eval%22%3A%221%22%2C%22fromPage%22%3A%22search%22%7D&pdp_npi=6%40dis%21BRL%2112.42%2112.42%21%21%2115.12%2115.12%21%402101eede17827565457506553e3bb1%2112000048087853910%21sea%21BR%21138446219%21X%211%210%21n_tag%3A-29919%3Bd%3Afc39af3%3Bm03_new_user%3A-29895&curPageLogUid=MC5nbzT4xkbo&utparam-url=scene%3Asearch%7Cquery_from%3A%7Cx_object_id%3A1005009147508182%7C_p_origin_prod%3A#nav-specification

#### ESC
- Tipo: ESC brushed para carro
- Alimentacao: LiPo 2S
- Corrente continua: 10 A a 20 A
- Funcoes: frente, re, freio
- BEC integrado: 5 V
- Corrente minima do BEC: 1 A
- Termo de busca: brushed ESC 2S 10A reverse brake 5V BEC

- Decidido:
Mini ESC Brushed Electric Speed Controller ESC Brush Electronic Motor Speed Controller Module 30A 4-8V For RC Car
https://pt.aliexpress.com/item/1005009648276627.html?spm=a2g0o.productlist.main.35.21d6uhZjuhZjyc&algo_pvid=de57448d-87f3-44e8-be56-53153ec45dc8&algo_exp_id=de57448d-87f3-44e8-be56-53153ec45dc8-34&pdp_ext_f=%7B%22order%22%3A%221298%22%2C%22spu_best_type%22%3A%22price%22%2C%22eval%22%3A%221%22%2C%22fromPage%22%3A%22search%22%7D&pdp_npi=6%40dis%21BRL%2114.59%2114.59%21%21%2117.77%2117.77%21%402101e8f317827584956935506e04b5%2112000049761815153%21sea%21BR%21138446219%21X%211%210%21n_tag%3A-29919%3Bd%3Afc39af3%3Bm03_new_user%3A-29895&curPageLogUid=Mw4Fzv09z9LE&utparam-url=scene%3Asearch%7Cquery_from%3A%7Cx_object_id%3A1005009648276627%7C_p_origin_prod%3A#nav-specification

#### Servo de direcao
- Alimentacao: 5 V
- Peso: 5 g a 9 g
- Torque minimo: 1.5 kg.cm
- Torque ideal: 1.8 kg.cm a 2.5 kg.cm
- Velocidade: 0.10 s a 0.14 s por 60 graus
- Tipo de engrenagem: nylon ou metal
- Termo de busca: 5V micro servo 9g 2kg

- Decidido:
MG90S Servo 1/4/10 Pcs Metal Gear Digital 9g Servo For RC Helicopter Airplane Boat Car RC Robot
https://pt.aliexpress.com/item/1005006860780474.html?spm=a2g0o.productlist.main.3.7f86Jv7MJv7Max&algo_pvid=5b6739a9-b3e5-427d-95f7-a870a72a6144&algo_exp_id=5b6739a9-b3e5-427d-95f7-a870a72a6144-2&pdp_ext_f=%7B%22order%22%3A%2213830%22%2C%22eval%22%3A%221%22%2C%22orig_sl_item_id%22%3A%221005006860780474%22%2C%22orig_item_id%22%3A%221005007522665750%22%2C%22fromPage%22%3A%22search%22%7D&pdp_npi=6%40dis%21BRL%21172.54%2186.29%21%21%21210.11%21105.08%21%4021033d1217827606755815155efa58%2112000038541824441%21sea%21BR%21138446219%21X%211%210%21n_tag%3A-29919%3Bd%3Afc39af3%3Bm03_new_user%3A-29895&curPageLogUid=eJby83GaapBH&utparam-url=scene%3Asearch%7Cquery_from%3A%7Cx_object_id%3A1005006860780474%7C_p_origin_prod%3A1005007522665750#nav-specification

#### Bateria
- Tipo: LiPo 2S
- Tensao nominal: 7.4 V
- Tensao maxima: 8.4 V
- Capacidade recomendada: 650 mAh a 850 mAh
- Taxa de descarga: 30C ou maior
- Conector: XT30
- Termo de busca: 2S LiPo 850mAh 30C XT30

#### Receptor
- Tipo: micro receiver compativel com FlySky AFHDS 2A
- Canais: 3 a 6
- Alimentacao: 4.8 V a 6.0 V pelo BEC do ESC
- Saida: PWM padrao RC
- Quantidade para compra: 1 unidade
- Termo de busca: FlySky AFHDS 2A micro receiver

#### Rolamentos
- Modelo: MR52ZZ
- Medida: 2 x 5 x 2.5 mm
- Tipo: blindado metal
- Uso previsto: eixo traseiro e pivots dianteiros
- Quantidade minima para compra: 10 unidades
- Quantidade mais segura: 20 unidades
- Termo de busca: MR52ZZ 2x5x2.5mm bearing

### 2. Itens mecanicos muito provaveis

#### Eixos
- Eixo traseiro: haste de aco 2 mm
- Linkagens: haste de 1.0 mm a 1.2 mm

#### Engrenagens
- Pinhao: modulo 0.5, 10T a 12T, furo 2.0 mm
- Coroa: modulo 0.5, 48T a 64T
- Relacao inicial: 5:1 a 7:1
- Termo de busca: 0.5 mod pinion 2mm bore
- Termo de busca: 0.5 mod spur gear 48T 64T

#### Cubos e fixacao de roda
- Cubos: eixo 2 mm com prisioneiro M2
- Rodas: 22 mm a 26 mm de diametro
- Largura sugerida: 8 mm a 12 mm

#### Linkagem de direcao
- Arame 0.8 mm a 1.2 mm
- Terminal M2 ou dobra tipo Z
- Servo horn compativel com o servo escolhido

### 3. Parafusos e fixadores

Voce ja tem M2 e M3, e para um chassi de 15 cm isso ainda cobre o projeto.

#### O que provavelmente sera usado de verdade
- M2: fixacao do servo, suportes pequenos, manga de eixo, tampa de transmissao, prisioneiros e partes finas impressas
- M3: longarinas, uniao principal do chassi, suporte de bateria, fixacao do ESC e do receptor se houver base mais robusta

#### Comprimentos uteis
- M2 x 4, 6, 8, 10 e 12 mm
- M3 x 6, 8, 10, 12 e 16 mm
- Porcas M2 e M3
- Arruelas M2 e M3
- Insertos roscados pequenos para plastico, se quiser manutencao melhor

## Regulacao de tensao

- Receptor: 5 V pelo BEC do ESC
- Servo: 5 V pelo BEC do ESC
- Regulador extra: nao necessario se o ESC tiver BEC integrado

## Lista de compra recomendada

### Comprar agora
- 2 a 3 motores brushed tamanho 130 para 2S, eixo 2.0 mm
- 1 a 2 ESCs brushed 2S com reverso e BEC 5 V
- 2 micro servos 5 V entre 5 g e 9 g, acima de 1.5 kg.cm
- 2 a 3 baterias LiPo 2S entre 650 mAh e 850 mAh, 30C ou maior, com XT30
- 1 micro receiver compativel com FlySky AFHDS 2A
- 10 a 20 rolamentos MR52ZZ 2 x 5 x 2.5 mm
- 2 hastes de aco 2 mm
- 1 kit de pinhoes 0.5M para eixo 2.0 mm
- 1 kit de coroas 0.5M 48T a 64T
- 1 jogo de cubos de roda para eixo 2 mm com prisioneiro M2
- 1 conjunto de linkagem de direcao 0.8 mm a 1.2 mm
- 1 jogo de conectores XT30 e fio de silicone 20 AWG a 24 AWG

### Comprar so se faltar no estoque
- Porcas M2 e M3
- Arruelas M2 e M3
- Parafusos M2 curtos em varios comprimentos
- Parafusos M3 medio e longos
- Espacadores plastico ou metal
- Servo horn extra

## BOM base

- FlySky FS-I6X
- Micro receiver compativel com AFHDS 2A
- Motor brushed tamanho 130, eixo 2.0 mm
- ESC brushed 2S com reverso e BEC 5 V
- Micro servo 5 V entre 5 g e 9 g
- Bateria LiPo 2S 650 mAh a 850 mAh com XT30
- Rolamentos MR52ZZ 2 x 5 x 2.5 mm
- Eixo de aco 2 mm
- Pinhao 0.5M 10T a 12T, furo 2.0 mm
- Coroa 0.5M 48T a 64T
- Cubos de roda 2 mm com prisioneiro M2
- Rodas 22 mm a 26 mm
- Linkagem 0.8 mm a 1.2 mm
- Parafusos M2 e M3






- Pneu(Tire)
- Roda(Wheel), tem q ter o furo central e o encaixe xegagonal para encaixar no cubo da roda
- Cubo da roda(WheelHub), da suporte para a rosca da roda, o eixo de roda passa por ela e ela fica encostada com espaçador no rolamento do manga do eixo
- Manga do eixo(SteeringKnuckle)
- HubCarrier


V1

Eixo 2mm
Rolamento 2x5x2.5mm

- Comprar cossinete 2mm

