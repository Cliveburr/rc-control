

https://kicad.org/download


Extensões de diagrama no VSCODE

Draw.io (hediet.vscode-drawio)
Comunidade: Muito grande — diagrams.net é amplamente usado; a extensão do Hediet é popular (repositório com milhares de stars).
Maturidade: Madura e estável, pensada para diagramas e documentação.
Atividade: Repositório ativo, releases e CI; mantém atualizações periódicas.
Componentes: Biblioteca de símbolos elétricos disponível; você pode importar/colar SVGs ou criar símbolos para ESP32, drivers e buck converters, mas não há footprint/netlist.
Exportação: PNG, SVG, PDF, XML (.drawio) e .drawio.png com dados embutidos — ótimo para documentação, não para fabricação.
Quando usar: se quer desenhar esquemas e guardar junto ao código no VSCode rapidamente (documentação/diagramas de fiação).

Circuit Diagram (circuitdiagram-vscode / circuit-diagram libraries)
Comunidade: Menor que draw.io; projeto nicho focado em diagramas eletrônicos simples.
Maturidade: Leve e suficiente para diagramas técnicos em docs, não EDA.
Atividade: atividade moderada/irregular (projetos menores).
Componentes: símbolos eletrônicos básicos; customização possível mas biblioteca limitada (ESP32 provavelmente como símbolo genérico).
Exportação: SVG/PNG/HTML; sem netlist nem gerbers.
Quando usar: se prefere diagramas rápidos em texto/markdown ou integração direta com docsites.

KiCad + KiCad Studio (oaslananka/kicad-studio para VS Code)
Comunidade: Muito grande — KiCad é ferramenta EDA open-source padrão; forte comunidade e bibliotecas extensas.
Maturidade: Produção/industrial — adequado para criar esquemáticos e preparar fabricação (PCB).
Atividade: KiCad é ativamente desenvolvido; a extensão KiCad Studio integra o workflow ao VS Code (repositorio separado, evolução mais recente).
Componentes: Bibliotecas extensas (muitos MCU/modules, drivers, conversores). ESP32 e drivers típicos têm símbolos/footprints comunitários; se faltar, cria-se o símbolo/footprint.
Exportação: Netlist, Gerber/Drill, BOM, STEP/3D, formatos industriais — ideal para passar a outro programa ou mandar fabricar.
Quando usar: se pretende, no futuro, fabricar PCBs ou precisar de netlist/BOM — é a opção mais completa (exige instalação do KiCad e curva de aprendizado).

SchemDraw (biblioteca Python)
Comunidade: Nicho (científico/descritivo), menor que opções acima.
Maturidade: Estável para criar imagens via código.
Atividade: mantido, bom para automação de diagramas.
Componentes: Shapes programáticos; dá para representar ESP32/stepper, mas sem footprint/netlist.
Exportação: SVG, PDF, PNG (gerado por script).
Quando usar: se quer gerar diagramas por script/automação (não interativo no VS Code).

Fritzing
Comunidade: Firme na comunidade hobbyista/maker; menos usado profissionalmente.
Maturidade: Fácil para breadboard/diagrama de ligação; UI feita para iniciantes.
Atividade: menor que KiCad; desenvolvimento e manutenção mais lentos.
Componentes: muitas peças comunitárias; ESP32 e módulos hobby geralmente disponíveis ou importáveis.
Exportação: Pode exportar PCB e gerar Gerbers (útil para fabricar), mas as bibliotecas/qualidade de footprints são mais limitadas que KiCad.
Quando usar: protótipos rápidos estilo breadboard e documentação para makers; menos recomendado para produção.
Recomendação curta