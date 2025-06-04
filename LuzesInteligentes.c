//Implementação desenvolvida por Andressa Sousa Fonseca
//Código de conexão MQTT adaptado de: https://github.com/raspberrypi/pico-examples/tree/master/pico_w/wifi/mqtt 

/*
O projeto desenvolvido consiste em um sistema de luzes inteligentes, no qual é possível controlar o estado e a intensidade
de luzes. Além disso, há a implementação de um sensor de movimento com base no joystick.
*/

#include "pico/stdlib.h"            // Biblioteca da Raspberry Pi Pico para funções padrão (GPIO, temporização, etc.)
#include "pico/cyw43_arch.h"        // Biblioteca para arquitetura Wi-Fi da Pico com CYW43
#include "pico/unique_id.h"         // Biblioteca com recursos para trabalhar com os pinos GPIO do Raspberry Pi Pico

#include "hardware/gpio.h"          // Biblioteca de hardware de GPIO
#include "hardware/irq.h"           // Biblioteca de hardware de interrupções
#include "hardware/adc.h"           // Biblioteca de hardware para conversão ADC

#include "lwip/apps/mqtt.h"         // Biblioteca LWIP MQTT -  fornece funções e recursos para conexão MQTT
#include "lwip/apps/mqtt_priv.h"    // Biblioteca que fornece funções e recursos para Geração de Conexões
#include "lwip/dns.h"               // Biblioteca que fornece funções e recursos suporte DNS:
#include "lwip/altcp_tls.h"         // Biblioteca que fornece funções e recursos para conexões seguras usando TLS:
//Configurações para a matriz de leds
#include "lib/matriz.h"
PIO pio = pio0; 
uint sm = 0;    

//definindo pinos do joystick
#define Eixo_X 26

//Pinos leds RGB
#define Led_blue    12
#define Led_green   11
#define Led_red     13


#define WIFI_SSID "SSID"                // Substitua pelo nome da sua rede Wi-Fi
#define WIFI_PASSWORD "PASSWWORD"       // Substitua pela senha da sua rede Wi-Fi
#define MQTT_SERVER "ID BROKER"         // Substitua pelo endereço do host - broket MQTT: Ex: 192.168.1.107
#define MQTT_USERNAME "Username"        // Substitua pelo nome da host MQTT - Username
#define MQTT_PASSWORD "password"        // Substitua pelo Password da host MQTT - credencial de acesso - caso exista

#ifndef MQTT_SERVER
#error Need to define MQTT_SERVER
#endif

// This file includes your client certificate for client server authentication
#ifdef MQTT_CERT_INC
#include MQTT_CERT_INC
#endif

#ifndef MQTT_TOPIC_LEN
#define MQTT_TOPIC_LEN 100
#endif

//Dados do cliente MQTT
typedef struct {
    mqtt_client_t* mqtt_client_inst;        
    struct mqtt_connect_client_info_t mqtt_client_info; 
    char data[MQTT_OUTPUT_RINGBUF_SIZE];   //define o tamanho do buffer para a mensagem
    char topic[MQTT_TOPIC_LEN];             //define o tamanho do tópico
    uint32_t len;                           
    ip_addr_t mqtt_server_address;          //vai armazenar o enderço de IP 
    bool connect_done;                      //aramazena o estado de conexão
    int subscribe_count;                    //aramazena a qunatidade de subscribes
    bool stop_client;                       
} MQTT_CLIENT_DATA_T;


#ifndef DEBUG_printf
#ifndef NDEBUG
#define DEBUG_printf printf
#else
#define DEBUG_printf(...)
#endif
#endif

#ifndef INFO_printf
#define INFO_printf printf
#endif

#ifndef ERROR_printf
#define ERROR_printf printf
#endif

// Temporização da coleta de temperatura - how often to measure our temperature
#define TEMP_WORKER_TIME_S 10

// Manter o programa ativo - keep alive in seconds
#define MQTT_KEEP_ALIVE_S 60

// QoS - mqtt_subscribe
// At most once (QoS 0)
// At least once (QoS 1)
// Exactly once (QoS 2)
#define MQTT_SUBSCRIBE_QOS 1        
#define MQTT_PUBLISH_QOS 1
#define MQTT_PUBLISH_RETAIN 0

// Tópico usado para: last will and testament
#define MQTT_WILL_TOPIC "/online"       
#define MQTT_WILL_MSG "0"
#define MQTT_WILL_QOS 1

//nome do dispositivo
#ifndef MQTT_DEVICE_NAME
#define MQTT_DEVICE_NAME "pico"
#endif

// Definir como 1 para adicionar o nome do cliente aos tópicos, para suportar vários dispositivos que utilizam o mesmo servidor
#ifndef MQTT_UNIQUE_TOPIC
#define MQTT_UNIQUE_TOPIC 0
#endif


// Requisição para publicar
static void pub_request_cb(__unused void *arg, err_t err);

// Topico MQTT
static const char *full_topic(MQTT_CLIENT_DATA_T *state, const char *name);

// Controle do LED 
static void control_led(MQTT_CLIENT_DATA_T *state, bool on);


// Requisição de Assinatura - subscribe
static void sub_request_cb(void *arg, err_t err);

// Requisição para encerrar a assinatura
static void unsub_request_cb(void *arg, err_t err);

// Tópicos de assinatura
static void sub_unsub_topics(MQTT_CLIENT_DATA_T* state, bool sub);

// Dados de entrada MQTT
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags);

// Dados de entrada publicados
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len);

//para monitorar o joyystick
static void sensor_movimento_worker_fn(async_context_t *context, async_at_time_worker_t *worker);
static async_at_time_worker_t sensor_movimento_worker = { .do_work = sensor_movimento_worker_fn };

// Conexão MQTT
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);

// Inicializar o cliente MQTT
static void start_client(MQTT_CLIENT_DATA_T *state);

// Call back com o resultado do DNS
static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg);


int main(void) {

    // Inicializa todos os tipos de bibliotecas stdio padrão presentes que estão ligados ao binário.
    stdio_init_all();
    INFO_printf("mqtt client starting\n");

    //Configurações para matriz de leds
    uint offset = pio_add_program(pio, &pio_matrix_program);
    pio_matrix_program_init(pio, sm, offset, MatrizLeds, 800000, IS_RGBW);

    // Inicializa o conversor ADC
    adc_init();

    //Inicializando entradas analógicas
    adc_gpio_init(Eixo_X);              //canal 1

    //Configurações para os LEDs RGB
    gpio_init(Led_blue);
    gpio_init(Led_green);
    gpio_init(Led_red);
    gpio_set_dir(Led_blue, GPIO_OUT);
    gpio_set_dir(Led_green, GPIO_OUT);
    gpio_set_dir(Led_red, GPIO_OUT);
    gpio_put(Led_blue,0);
    gpio_put(Led_green,0);
    gpio_put(Led_red,0);


    // Cria registro com os dados do cliente
    static MQTT_CLIENT_DATA_T state;

    // Inicializa a arquitetura do cyw43
    if (cyw43_arch_init()) {
        panic("Failed to inizialize CYW43");
    }

    // Usa identificador único da placa
    char unique_id_buf[5];
    pico_get_unique_board_id_string(unique_id_buf, sizeof(unique_id_buf));
    for(int i=0; i < sizeof(unique_id_buf) - 1; i++) {
        unique_id_buf[i] = tolower(unique_id_buf[i]);
    }

    // Gera nome único, Ex: pico1234
    char client_id_buf[sizeof(MQTT_DEVICE_NAME) + sizeof(unique_id_buf) - 1];
    memcpy(&client_id_buf[0], MQTT_DEVICE_NAME, sizeof(MQTT_DEVICE_NAME) - 1);
    memcpy(&client_id_buf[sizeof(MQTT_DEVICE_NAME) - 1], unique_id_buf, sizeof(unique_id_buf) - 1);
    client_id_buf[sizeof(client_id_buf) - 1] = 0;
    INFO_printf("Device name %s\n", client_id_buf);

    //define os parâmetros da struct state
    state.mqtt_client_info.client_id = client_id_buf;
    state.mqtt_client_info.keep_alive = MQTT_KEEP_ALIVE_S; // Keep alive in sec
#if defined(MQTT_USERNAME) && defined(MQTT_PASSWORD)
    state.mqtt_client_info.client_user = MQTT_USERNAME;
    state.mqtt_client_info.client_pass = MQTT_PASSWORD;
#else
    state.mqtt_client_info.client_user = NULL;
    state.mqtt_client_info.client_pass = NULL;
#endif
    static char will_topic[MQTT_TOPIC_LEN];
    strncpy(will_topic, full_topic(&state, MQTT_WILL_TOPIC), sizeof(will_topic));
    state.mqtt_client_info.will_topic = will_topic;
    state.mqtt_client_info.will_msg = MQTT_WILL_MSG;
    state.mqtt_client_info.will_qos = MQTT_WILL_QOS;
    state.mqtt_client_info.will_retain = true;
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    // TLS enabled
#ifdef MQTT_CERT_INC
    static const uint8_t ca_cert[] = TLS_ROOT_CERT;
    static const uint8_t client_key[] = TLS_CLIENT_KEY;
    static const uint8_t client_cert[] = TLS_CLIENT_CERT;
    // This confirms the indentity of the server and the client
    state.mqtt_client_info.tls_config = altcp_tls_create_config_client_2wayauth(ca_cert, sizeof(ca_cert),
            client_key, sizeof(client_key), NULL, 0, client_cert, sizeof(client_cert));
#if ALTCP_MBEDTLS_AUTHMODE != MBEDTLS_SSL_VERIFY_REQUIRED
    WARN_printf("Warning: tls without verification is insecure\n");
#endif
#else
    state->client_info.tls_config = altcp_tls_create_config_client(NULL, 0);
    WARN_printf("Warning: tls without a certificate is insecure\n");
#endif
#endif

    //executa 3 tentaivas de conectar ao wifi-fi
    // Conectar à rede WiFI - fazer um loop até que esteja conectado
    int tentativas = 1;

    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000) && tentativas<4) {
        panic("Failed to connect");
        //printf("\nTentativa");
        tentativas++;
    }
    INFO_printf("\nConnected to Wifi\n");

    //Faz um pedido de DNS para o endereço IP do servidor MQTT
    cyw43_arch_lwip_begin();
    int err = dns_gethostbyname(MQTT_SERVER, &state.mqtt_server_address, dns_found, &state);
    cyw43_arch_lwip_end();

    // Se tiver o endereço, inicia o cliente
    if (err == ERR_OK) {
        start_client(&state);
    } else if (err != ERR_INPROGRESS) { // ERR_INPROGRESS means expect a callback
        panic("dns request failed");
    }

    // Loop condicionado a conexão mqtt
    while (!state.connect_done || mqtt_client_is_connected(state.mqtt_client_inst)) {
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(10000));
    }

    INFO_printf("mqtt client exiting\n");
    return 0;
}


// Requisição para publicar
static void pub_request_cb(__unused void *arg, err_t err) {
    if (err != 0) {
        ERROR_printf("pub_request_cb failed %d", err);
    }
}

//Topico MQTT
static const char *full_topic(MQTT_CLIENT_DATA_T *state, const char *name) {
#if MQTT_UNIQUE_TOPIC
    static char full_topic[MQTT_TOPIC_LEN];
    snprintf(full_topic, sizeof(full_topic), "/%s%s", state->mqtt_client_info.client_id, name);
    return full_topic;
#else
    return name;
#endif
}

//Variáveis para salvar referências para construir a matriz de leds
static uint estado_sala = 0;
static uint estado_quarto = 0;
static uint intensidade_sala = 0;
static uint intensidade_quarto = 0;

//Controla os pontos da matriz para a sala
static void control_sala(MQTT_CLIENT_DATA_T *state, bool on) {
    if (on){
        estado_sala = 1;            //Salva a referência para a sala
        luzes_Inteligentes(estado_quarto,estado_sala, intensidade_quarto, 0, true); //Chama a função que constrói a mastriz
    }else{
        estado_sala = 0;
        luzes_Inteligentes(estado_quarto,estado_sala, intensidade_quarto, 0, true);
    };
    //OBSERVAÇÃO: Uma publicação dentro dessa função pode ocasionar um loop
};

//Controla os pontos da matriz para a intensidade da sala
static void control_matriz_intensidade_sala(MQTT_CLIENT_DATA_T *state, uint intensity) {
    char tipo_intensidade[3][7] = {"mínima","média","máxima"};          //Para publicar o estado de intensidade
    const char* message = tipo_intensidade[intensity-1];                //Salva a mensagem correposndente
    luzes_Inteligentes(estado_quarto,estado_sala, intensidade_quarto, intensity-1, true);   //Chama a função que constrói a mastriz
    intensidade_sala = intensity-1;                                                         //Salva a referência para a sala
    mqtt_publish(state->mqtt_client_inst, full_topic(state, "casa/sala/intensidade"), message, strlen(message), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
}

//Controla os pontos da matriz para o quarto
static void control_quarto(MQTT_CLIENT_DATA_T *state, bool on) {
    if (on){
        estado_quarto = 1;
        luzes_Inteligentes(estado_quarto,estado_sala, 0, intensidade_sala, false);
    }else{
        estado_quarto = 0;
        luzes_Inteligentes(estado_quarto,estado_sala, 0, intensidade_sala, false);
    };

};

//Controla os pontos da matriz para intensidade do quarto
static void control_matriz_intensidade_quarto(MQTT_CLIENT_DATA_T *state, uint intensity) {
    char tipo_intensidade[3][7] = {"mínima","média","máxima"};
    const char* message = tipo_intensidade[intensity-1];
    luzes_Inteligentes(estado_sala,estado_sala, intensity-1, intensidade_sala, false);
    intensidade_quarto = intensity-1;
    mqtt_publish(state->mqtt_client_inst, full_topic(state, "casa/quarto/intensidade"), message, strlen(message), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
}

//Controla os pontos da matriz para o jardim
static void control_luz_jardim(MQTT_CLIENT_DATA_T *state, bool on) {
    if (on){
        gpio_put(Led_blue,0.5);    //Se on for true, liga o led com cor branca
        gpio_put(Led_green,0.5);
        gpio_put(Led_red,0.5);
    }else{
        gpio_put(Led_blue,0);      //Se on for false, desliga
        gpio_put(Led_green,0);
        gpio_put(Led_red,0);
    }
}


//Função para ler dados do joystick
static void leitura_sensor_movimento(MQTT_CLIENT_DATA_T *state){
    adc_select_input(0);                                                    //Seleciona o canal para ler o adc
    const char *movimento_key = full_topic(state, "casa/jardim/movimento"); //Para publicar
    uint leitura_atual = adc_read();                                        //ler o dado do joystick
    int verificacao, correspondencias=0;            //Variáveis de controle
    for(int i =0; i<10;i++){                        //For para ignorar mudanças bruscas emomentâneas
        verificacao = adc_read();
        if(verificacao>= leitura_atual-20 && verificacao <= leitura_atual+20){
            correspondencias ++;
        };
        sleep_ms(50);
    };
    if(correspondencias<=5){                        //Se a maior parte das leituras forem diferentes, 
        char *message2 = "On";                      //considera-se que houve movimento
        char *message = "Movimento detectado";
        INFO_printf("Publishing %s\n", message);
        //Publica o estado do movimento
        mqtt_publish(state->mqtt_client_inst, movimento_key, message, strlen(message), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
        //Publica o estado desejado para o led
        mqtt_publish(state->mqtt_client_inst, "casa/jardim/state", message2, strlen(message2), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
    }else{
        char *message2 = "Off";
        char *message = "Movimento não detectado";
        INFO_printf("Publishing %s\n", message);
        mqtt_publish(state->mqtt_client_inst, movimento_key, message, strlen(message), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
        mqtt_publish(state->mqtt_client_inst, "casa/jardim/state", message2, strlen(message2), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
    };
};


// Requisição de Assinatura - subscribe
static void sub_request_cb(void *arg, err_t err) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    if (err != 0) {
        panic("subscribe request failed %d", err);
    }
    state->subscribe_count++;
}

// Requisição para encerrar a assinatura
static void unsub_request_cb(void *arg, err_t err) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    if (err != 0) {
        panic("unsubscribe request failed %d", err);
    }
    state->subscribe_count--;
    assert(state->subscribe_count >= 0);

    // Stop if requested
    if (state->subscribe_count <= 0 && state->stop_client) {
        mqtt_disconnect(state->mqtt_client_inst);
    }
}

// Tópicos 
static void sub_unsub_topics(MQTT_CLIENT_DATA_T* state, bool sub) {
    mqtt_request_cb_t cb = sub ? sub_request_cb : unsub_request_cb;
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "casa/sala/intensidade"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "casa/sala/state"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "casa/quarto/intensidade"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "casa/quarto/state"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "casa/jardim/state"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/print"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/ping"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/exit"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
}



// Dados de entrada MQTT
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
#if MQTT_UNIQUE_TOPIC
    const char *basic_topic = state->topic + strlen(state->mqtt_client_info.client_id) + 1;
#else
    const char *basic_topic = state->topic;
#endif
    strncpy(state->data, (const char *)data, len);
    state->len = len;
    state->data[len] = '\0';

    DEBUG_printf("Topic: %s, Message: %s\n", state->topic, state->data);

    //Verifica qual tópico foi publicado, depois a ação desejada e, por fim, chama a função correspondente
    if (strcmp(basic_topic, "/print") == 0) {
        INFO_printf("%.*s\n", len, data);
    } else if (strcmp(basic_topic, "/ping") == 0) {
        char buf[11];
        snprintf(buf, sizeof(buf), "%u", to_ms_since_boot(get_absolute_time()) / 1000);
        mqtt_publish(state->mqtt_client_inst, full_topic(state, "/uptime"), buf, strlen(buf), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
    } else if (strcmp(basic_topic, "/exit") == 0) {
        state->stop_client = true; // stop the client when ALL subscriptions are stopped
        sub_unsub_topics(state, false); // unsubscribe
    }   
     else if (strcmp(basic_topic, "casa/sala/state") == 0) {    //Se o tópico for esse, só há duas ações On ou Off 
        bool estado_sala = false;
        if (lwip_stricmp((const char *)state->data, "On") == 0) //Identifica-se a ação desejada
            estado_sala = true;                                 //Salva na variável
        else if (lwip_stricmp((const char *)state->data, "Off") == 0)
            estado_sala = false;
        control_sala(state, estado_sala);                       //Chama a função passando o estado desejado
    }else if (strcmp(basic_topic, "casa/sala/intensidade") == 0) {
        if (lwip_stricmp((const char *)state->data, "1" ) == 0){        //Para intensidade tem-se três opções
            control_matriz_intensidade_sala(state, 1); 
        }else if (lwip_stricmp((const char *)state->data, "2") == 0)
            control_matriz_intensidade_sala(state, 2);
        else if (lwip_stricmp((const char *)state->data, "3") == 0)
            control_matriz_intensidade_sala(state, 3);
    }else if (strcmp(basic_topic, "casa/quarto/state") == 0) {
         bool estado_quarto = false;
        if (lwip_stricmp((const char *)state->data, "On") == 0)
            estado_quarto = true;
        else if (lwip_stricmp((const char *)state->data, "Off") == 0)
            estado_quarto = false;
        control_quarto(state, estado_quarto);
    }else if (strcmp(basic_topic, "casa/quarto/intensidade") == 0) {
        if (lwip_stricmp((const char *)state->data, "1" ) == 0){
            control_matriz_intensidade_quarto(state, 1); 
        }else if (lwip_stricmp((const char *)state->data, "2") == 0)
            control_matriz_intensidade_quarto(state, 2);
        else if (lwip_stricmp((const char *)state->data, "3") == 0)
            control_matriz_intensidade_quarto(state, 3);
    }else if (strcmp(basic_topic, "casa/jardim/state") == 0) {
        if (lwip_stricmp((const char *)state->data, "On" ) == 0){
            control_luz_jardim(state, true);
        }else if (lwip_stricmp((const char *)state->data, "Off") == 0)
            control_luz_jardim(state, false);
    };

};

// Dados de entrada publicados
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    strncpy(state->topic, topic, sizeof(state->topic));
}


//  Permite as leituras do joystick periodicamente
static void sensor_movimento_worker_fn(async_context_t *context, async_at_time_worker_t *worker) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)worker->user_data;
    leitura_sensor_movimento(state);
    async_context_add_at_time_worker_in_ms(context, worker, TEMP_WORKER_TIME_S * 1000);
}


// Conexão MQTT
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    if (status == MQTT_CONNECT_ACCEPTED) {
        state->connect_done = true;
        sub_unsub_topics(state, true); // subscribe;

        // indicate online
        if (state->mqtt_client_info.will_topic) {
            mqtt_publish(state->mqtt_client_inst, state->mqtt_client_info.will_topic, "1", 1, MQTT_WILL_QOS, true, pub_request_cb, state);
        }

        //MODIFICAÇÃO
        sensor_movimento_worker.user_data = state;
        async_context_add_at_time_worker_in_ms(cyw43_arch_async_context(), &sensor_movimento_worker, 500);

    } else if (status == MQTT_CONNECT_DISCONNECTED) {
        if (!state->connect_done) {
            panic("Failed to connect to mqtt server");
        }
    }
    else {
        panic("Unexpected status");
    }
}

// Inicializar o cliente MQTT
static void start_client(MQTT_CLIENT_DATA_T *state) {
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    const int port = MQTT_TLS_PORT;
    INFO_printf("Using TLS\n");
#else
    const int port = MQTT_PORT;
    INFO_printf("Warning: Not using TLS\n");
#endif

    state->mqtt_client_inst = mqtt_client_new();
    if (!state->mqtt_client_inst) {
        panic("MQTT client instance creation error");
    }
    INFO_printf("IP address of this device %s\n", ipaddr_ntoa(&(netif_list->ip_addr)));
    INFO_printf("Connecting to mqtt server at %s\n", ipaddr_ntoa(&state->mqtt_server_address));

    cyw43_arch_lwip_begin();
    if (mqtt_client_connect(state->mqtt_client_inst, &state->mqtt_server_address, port, mqtt_connection_cb, state, &state->mqtt_client_info) != ERR_OK) {
        panic("MQTT broker connection error");
    }
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    // This is important for MBEDTLS_SSL_SERVER_NAME_INDICATION
    mbedtls_ssl_set_hostname(altcp_tls_context(state->mqtt_client_inst->conn), MQTT_SERVER);
#endif
    mqtt_set_inpub_callback(state->mqtt_client_inst, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, state);
    cyw43_arch_lwip_end();
}

// Call back com o resultado do DNS
static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg) {
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T*)arg;
    if (ipaddr) {
        state->mqtt_server_address = *ipaddr;
        start_client(state);
    } else {
        panic("dns request failed");
    }
}