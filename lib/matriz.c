#include "matriz.h"
//#include "math.h"
#include <stdlib.h>
#include "pico/stdlib.h"
#include <stdio.h>


//Retorno o valor binário para a cor passada por parâmetro
uint32_t cor_binario (double b, double r, double g)
{
unsigned char R, G, B;
R = r * 255; 
G = g * 255;  
B = b * 255;
return (G << 24) | (R << 16) | (B << 8);
};

//Função responsável por acender os leds desejados 
void ligar_leds(Matriz_leds matriz){
  //Primeiro for para percorrer cada linha
  for (int linha =4;linha>=0;linha--){
      /*
      Devido à ordem de disposição dos leds na matriz de leds 5X5, é necessário
      ter o cuidado para imprimir o desenho na orientação correta. Assim, o if abaixo permite o 
      desenho saia extamente como projetado.
      */

      if(linha%2){                             //Se verdadeiro, a numeração das colunas começa em 4 e decrementam
          for(int coluna=0;coluna<5;coluna++){
              uint32_t cor = cor_binario(matriz[linha][coluna].blue,matriz[linha][coluna].red,matriz[linha][coluna].green);
              pio_sm_put_blocking(pio, sm, cor);
          };
      }else{                                      //Se falso, a numeração das colunas começa em 0 e incrementam
          for(int coluna=4;coluna>=0;coluna--){
              uint32_t cor = cor_binario(matriz[linha][coluna].blue,matriz[linha][coluna].red,matriz[linha][coluna].green);
              pio_sm_put_blocking(pio, sm, cor);
          };
      };
  };
};

//Função para a matriz de Leds
void luzes_Inteligentes(uint luzes_quarto,uint luzes_sala, uint modo_quarto, uint modo_sala, bool sala){

    //uint modo = 0;
    COR_RGB apagado = {0.0,0.0,0.0};    //Define os padrões para desligar um led
    float intensidades []= {0.01,0.2,0.7};     //Vetor com as intensidades disponíveis
    
    //Vetor para permitir alterna o estado do led: posição 0 - led desligado; posição 1 - led branco
    //Estando no vetor facilita as modificações ao atualizar a matriz
    COR_RGB iluminacao_quarto [] = {{0.0,0.0,0.0},{0.5*intensidades[modo_quarto],0.5*intensidades[modo_quarto],0.5*intensidades[modo_quarto]}}; 
    COR_RGB iluminacao_sala [] = {{0.0,0.0,0.0},{0.5*intensidades[modo_sala],0.5*intensidades[modo_sala],0.5*intensidades[modo_sala]}}; 

    //Matriz inicial que será modificada para a sala
    Matriz_leds iluminacao_s = {{apagado,apagado,apagado,apagado,apagado},
                            {apagado,iluminacao_quarto[luzes_quarto], iluminacao_quarto[luzes_quarto],iluminacao_quarto[luzes_quarto],apagado},
                            {apagado,iluminacao_quarto[luzes_quarto], apagado,iluminacao_quarto[luzes_quarto],apagado},
                            {apagado,iluminacao_quarto[luzes_quarto], iluminacao_quarto[luzes_quarto],iluminacao_quarto[luzes_quarto],apagado},
                            {apagado,apagado,apagado,apagado,apagado}};
    
    //Matriz inicial que será modificada para o quarto
    Matriz_leds iluminacao_q = {{iluminacao_sala[luzes_sala],iluminacao_sala[luzes_sala],iluminacao_sala[luzes_sala],iluminacao_sala[luzes_sala],iluminacao_sala[luzes_sala]},
                            {iluminacao_sala[luzes_sala],apagado,apagado,apagado,iluminacao_sala[luzes_sala]},
                            {iluminacao_sala[luzes_sala],apagado,apagado,apagado,iluminacao_sala[luzes_sala]},
                            {iluminacao_sala[luzes_sala],apagado,apagado,apagado,iluminacao_sala[luzes_sala]},
                            {iluminacao_sala[luzes_sala],iluminacao_sala[luzes_sala],iluminacao_sala[luzes_sala],iluminacao_sala[luzes_sala],iluminacao_sala[luzes_sala]}};

    //variáveis de controle para o desenho, ao modificá-las é possível aumentar ou diminuir o quadrado da animação
    uint c=0;                   //Variável para posicionar as colunas
    uint limite = 4;            //Armazena a posição da coluna mais externa do desenho
    uint leds_linha = 3;        //Quantidade de passsos para ligar os leds da linha
    uint primeira_linha = 0;    //posição da primeira linha
    uint ultima_linha = 4;      //posição da última linha


    for(int i = 0; i<5;i++){ //Percorre cada linha
        for(int j = 0; j< leds_linha; j++){ //Percorre cada coluna - leds_linha é a quantidade q liga em cada linha
            if(i==primeira_linha){
                if(j==0){
                    c = 4/2;    //posiciona no meio da linha
                    if(sala)
                        iluminacao_s[i][c] = iluminacao_sala[luzes_sala];
                    else
                        iluminacao_q[i+1][c] = iluminacao_quarto[luzes_quarto];
                }else{
                    if(sala){
                        iluminacao_s[i][c+j] =  iluminacao_sala[luzes_sala];     //liga uma posição a mais para a direita
                        iluminacao_s[i][c-j] = iluminacao_sala[luzes_sala];    //liga uma posição a menos para a esquerda
                    }else if(j==((limite-2)/2)){
                        iluminacao_q[i+1][c+j] = iluminacao_quarto[luzes_quarto];     //liga uma posição a mais para a direita
                        iluminacao_q[i+1][c-j] = iluminacao_quarto[luzes_quarto];    //liga uma posição a menos para a esquerda
                    };
                };
                if(sala)
                    ligar_leds(iluminacao_s); //Mostra a nova construção
                else
                    ligar_leds(iluminacao_q); //Mostra a nova construção
                sleep_ms(50);
            }else if (i>primeira_linha && i<ultima_linha){
                int borda = abs(limite-4);
                
                
                if(sala){
                    iluminacao_s[i][limite] = iluminacao_sala[luzes_sala];    //liga as bordas de cada linha
                    iluminacao_s[i][borda] = iluminacao_sala[luzes_sala];
                    ligar_leds(iluminacao_s); //Mostra a nova construção
                }else{
                    iluminacao_q[i][limite-1] = iluminacao_quarto[luzes_quarto];    //liga as bordas de cada linha
                    iluminacao_q[i][borda+1] = iluminacao_quarto[luzes_quarto];
                    ligar_leds(iluminacao_q); //Mostra a nova construção
                }
                    sleep_ms(50);
            }else if(i==ultima_linha){
                if(j<c && sala){
                    printf("j: %d e c: %d\n", j, c);
                    iluminacao_s[i][j] = iluminacao_sala[luzes_sala];
                    iluminacao_s[i][limite-j] = iluminacao_sala[luzes_sala];
                    /*if(j<c){
                        iluminacao[i-1][(limite-1)-j] = iluminacao_quarto[luzes_quarto];
                        iluminacao[i-1][j+1] = iluminacao_quarto[luzes_quarto];
                    };*/
                    ligar_leds(iluminacao_s); //Mostra a nova construção
                    sleep_ms(75);
                }else if(j==c){
                    if(sala){
                        iluminacao_s[i][j] = iluminacao_sala[luzes_sala];
                        ligar_leds(iluminacao_s); //Mostra a nova construção
                    }else{
                        iluminacao_q[i-1][j] = iluminacao_quarto[luzes_quarto];
                        ligar_leds(iluminacao_q); //Mostra a nova construção
                    }
                    sleep_ms(75);
                    break;
                };
            };
        };
        //ligar_leds(iluminacao); //Mostra a nova construção
        //sleep_ms(75);
    };
};