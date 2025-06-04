# LuzesInteligentesMQTT
Repositório destinado ao desenvolvimento da Tarefa sobre utilização do protocolo MQTT -- Residência em Software Embarcado - Embarca Tech TIC 37

__Responsável pelo desenvolvimento:__
Andressa Sousa Fonseca

## Componentes utilizados
1) Matriz de LEDs
2) LEDs RGB
3) Joystick
4) IoT MQTT Panel
5) Termux
6) MQTT Explorer
7) Mosquitto

## Destalhes do projeto

O projeto utiliza o protocolo MQTT para controlar a matriz de leds e os led RGB da placa BitDogLab. Esse sistema representa o controle de iluminação de uma residência, no qual é possível ligar e desligar luzes, controlar intensidade e receber os dados de um sensor de movimento remotamente.
<br>
1) Especificações da Matriz de Leds:
- Setorizada para representações dois cômodos, o quarto e a sala

2) Especificações dos LEDs RGB:
- Representa outro cômodo, o jardim e pode ser aciona pelo sensor de mvimento.
  
3) Especificações do Joystick:
- Simula o sensor de movimento.
  
<br>Os tópicos utilizados foram:
- casa/sala/intensidade
- casa/sala/state
- casa/quarto/intensidade
- casa/quarto/state
- casa/jardim/state
- casa/jardim/movimento

  <br>O termx foi utilizado para instalar e configurar o mosquitto, o broker. O IoT MQTT Panel permitiu enviar comando e desenvolver uma interface amigável para o controle dos leds. Por sua vez, o MQTT Explorer permitu que o computador funcionasse como um cliente que podia receber os dados e também enviar publicações ao tópicos.


   

   

