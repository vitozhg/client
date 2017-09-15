#include "mbed.h"
#include "C12832.h"
#include "LM75B.h"
#include "EthernetInterface.h"

extern "C" {
    #include "liblwm2m.h"
    //External API
    extern lwm2m_object_t * get_object_device(void);
    extern void free_object_device(lwm2m_object_t * objectP);


#ifdef SUPPORT_SECURITY
    extern lwm2m_object_t * get_server_object(int serverId,
                                   const char* binding,
                                   int lifetime,
                                   bool storing);
    extern lwm2m_object_t * get_security_object(int serverId,
                                     const char* serverUri,
                                     char * bsPskId,
                                     char * psk,
                                     uint16_t pskLen,
                                     bool isBootstrap);

#else
	extern lwm2m_object_t * get_server_object(void);
	extern lwm2m_object_t * get_security_object(void);
#endif

    extern void free_server_object(lwm2m_object_t * object);
    extern void free_security_object(lwm2m_object_t * objectP);
    extern char * get_server_uri(lwm2m_object_t * objectP, uint16_t secObjInstID);
}
#ifdef ENABLE_PWM_RGB_LED
lwm2m_object_t * get_object_rgb_led(void);
#endif
lwm2m_object_t * get_object_temperature(void);
/*********************************************/
/* Global definitions*/
#define ENDPOINT_NAME "TINYCLOUD_DEMO"

/* Connect to the Leshan test server as default: http://leshan.eclipse.org */
//#define LESHAN_SERVER "5.39.83.206"

/* Connect to tiny cloud test server: 222.172.106.72:5683 */
#define LESHAN_SERVER "222.172.106.72"
#define LESHAN_PORT 5683
#define LESHAN_PORT_STR "5683"

#define UDP_PORT 5683

#ifdef ENABLE_PWM_RGB_LED
#define DEVICE_OBJ_NUM 5
#else
#define DEVICE_OBJ_NUM 4
#endif
#define LOCAL_PORT 5683

//#define UDP_TIMEOUT 3000    //ms
//#define LWM2M_CLIENT_TIMEOUT   10
//#define LWM2M_SERVER_TIMEOUT   20

#define UDP_TIMEOUT 1000    //ms
#define LWM2M_CLIENT_TIMEOUT   60
#define LWM2M_SERVER_TIMEOUT   60



// LCD 128X32
C12832 lcd(p5, p7, p6, p8, p11);
// Sensor of temperature
LM75B sensor_temp(p28,p27);
// Network interface
EthernetInterface eth;
// UDP Socket
UDPSocket udp;
// Serial Port
Serial pc(USBTX, USBRX);
typedef struct
{
    lwm2m_object_t * securityObjP;
    UDPSocket sock;
    /*connection_t * connList;*/
} client_data_t;

//Functions
void print_state(lwm2m_context_t * lwm2mH)
{
    lwm2m_server_t * targetP;

    fprintf(stderr, "State: ");

    switch(lwm2mH->state)
    {
    case STATE_INITIAL:
        fprintf(stderr, "STATE_INITIAL");
        break;
    case STATE_BOOTSTRAP_REQUIRED:
        fprintf(stderr, "STATE_BOOTSTRAP_REQUIRED");
        break;
    case STATE_BOOTSTRAPPING:
        fprintf(stderr, "STATE_BOOTSTRAPPING");
        break;
    case STATE_REGISTER_REQUIRED:
        fprintf(stderr, "STATE_REGISTER_REQUIRED");
        break;
    case STATE_REGISTERING:
        fprintf(stderr, "STATE_REGISTERING");
        break;
    case STATE_READY:
        fprintf(stderr, "STATE_READY");
        break;
    default:
        fprintf(stderr, "Unknown !");
        break;
    }
    fprintf(stderr, "\r\n");

    targetP = lwm2mH->bootstrapServerList;

    if (lwm2mH->bootstrapServerList == NULL)
    {
        fprintf(stderr, "No Bootstrap Server.\r\n");
    }
    else
    {
        fprintf(stderr, "Bootstrap Servers:\r\n");
        for (targetP = lwm2mH->bootstrapServerList ; targetP != NULL ; targetP = targetP->next)
        {
            fprintf(stderr, " - Security Object ID %d", targetP->secObjInstID);
            fprintf(stderr, "\tHold Off Time: %lu s", (unsigned long)targetP->lifetime);
            fprintf(stderr, "\tstatus: ");
            switch(targetP->status)
            {
            case STATE_DEREGISTERED:
                fprintf(stderr, "DEREGISTERED\r\n");
                break;
            case STATE_BS_HOLD_OFF:
                fprintf(stderr, "CLIENT HOLD OFF\r\n");
                break;
            case STATE_BS_INITIATED:
                fprintf(stderr, "BOOTSTRAP INITIATED\r\n");
                break;
            case STATE_BS_PENDING:
                fprintf(stderr, "BOOTSTRAP PENDING\r\n");
                break;
            case STATE_BS_FINISHED:
                fprintf(stderr, "BOOTSTRAP FINISHED\r\n");
                break;
            case STATE_BS_FAILED:
                fprintf(stderr, "BOOTSTRAP FAILED\r\n");
                break;
            default:
                fprintf(stderr, "INVALID (%d)\r\n", (int)targetP->status);
            }
            fprintf(stderr, "\r\n");
        }
    }

    if (lwm2mH->serverList == NULL)
    {
        fprintf(stderr, "No LWM2M Server.\r\n");
    }
    else
    {
        fprintf(stderr, "LWM2M Servers:\r\n");
        for (targetP = lwm2mH->serverList ; targetP != NULL ; targetP = targetP->next)
        {
            fprintf(stderr, " - Server ID %d", targetP->shortID);
            fprintf(stderr, "\tstatus: ");
            switch(targetP->status)
            {
            case STATE_DEREGISTERED:
                fprintf(stderr, "DEREGISTERED\r\n");
                break;
            case STATE_REG_PENDING:
                fprintf(stderr, "REGISTRATION PENDING\r\n");
                break;
            case STATE_REGISTERED:
                fprintf(stderr, "REGISTERED\tlocation: \"%s\"\tLifetime: %lus\r\n", targetP->location, (unsigned long)targetP->lifetime);
                break;
            case STATE_REG_UPDATE_PENDING:
                fprintf(stderr, "REGISTRATION UPDATE PENDING\r\n");
                break;
            case STATE_REG_UPDATE_NEEDED:
                fprintf(stderr, "REGISTRATION UPDATE REQUIRED\r\n");
                break;
            case STATE_DEREG_PENDING:
                fprintf(stderr, "DEREGISTRATION PENDING\r\n");
                break;
            case STATE_REG_FAILED:
                fprintf(stderr, "REGISTRATION FAILED\r\n");
                break;
            default:
                fprintf(stderr, "INVALID (%d)\r\n", (int)targetP->status);
            }
            fprintf(stderr, "\r\n");
        }
    }
}

void debug_dump(uint8_t * buffer, size_t length)
{
    int i;
    printf("\n--------------------------\n"); 
    for(i=0;i<length;i++){
        printf("0x%2x ",buffer[i]);
        if(i%16==15)printf("\n");    
    }   
    printf("\n--------------------------\n"); 
}
void test_tcp(EthernetInterface eth)
{
    TCPSocket tcp;    
    tcp.open(&eth);
    tcp.connect("developer.mbed.org", 80);

    // Send a simple http request
    char sbuffer[] = "GET / HTTP/1.1\r\nHost: developer.mbed.org\r\n\r\n";
    int scount = tcp.send(sbuffer, sizeof sbuffer);
 
    lcd.cls();   
    lcd.locate(0,0);  
    lcd.printf("TCP TESTING: ");
    lcd.printf("SEND %d\n[%.*s]\n", scount, strstr(sbuffer, "\r\n")-sbuffer, sbuffer); 
    wait(3.0);
    
    // Recieve a simple http response and print out the response line
    char rbuffer[64];
    int rcount = tcp.recv(rbuffer, sizeof rbuffer);
    lcd.cls(); 
    lcd.locate(0,0);
    lcd.printf("TCP TESTING: ");
    lcd.printf("RECV %d\n[%.*s]\n", rcount, strstr(rbuffer, "\r\n")-rbuffer, rbuffer);
    wait(3.0);
    lcd.cls();
    // Close the socket to return its memory and bring down the network interface
    tcp.close();   
}


int init_network()
{
    int ret = 0;

    //try to connect and get ip via DHCP.
    lcd.locate(0,10);
    lcd.printf("obtaining ip address...\n");
    ret = eth.connect();
    if(ret!=0){
        lcd.printf("DHCP Error - No IP");
        return ret;
    }
    lcd.printf("IP is %s\n", eth.get_ip_address());
    //wait(2.0);
    
    //test code which only used to verify the connect functinon
    //test_tcp(eth);
    //test_udp(eth);
    
    udp.open(&eth);
    udp.set_timeout(UDP_TIMEOUT);
    udp.bind(UDP_PORT);
    
    return ret;    
}

int init_display()
{
    int ret = 0;
    //Bootup and display initial information
    lcd.cls();
    lcd.locate(0,0);
    lcd.printf(ENDPOINT_NAME);
    lcd.locate(0,10);
    lcd.printf("starting...");
    
    return ret; 
}


coap_status_t lwm2m_buffer_send(void * sessionH,uint8_t * buffer,size_t length,void * userdata)
{
    int ret = 0;
    
    SocketAddress * addr = (SocketAddress*) sessionH;
    printf("@@@sessionH = %d\n",sessionH);
    printf("\n>>>>>Send packet to: %s of port %d, size: %d\n\n", addr->get_ip_address(), addr->get_port(),length);
    //debug_dump(buffer,length);
    
    ret = udp.sendto(addr->get_ip_address(), addr->get_port(), (void *)buffer, (int)length);
    if(ret!=length)
    {
        return COAP_500_INTERNAL_SERVER_ERROR;
    }
    
    return COAP_NO_ERROR;
}

void get_temperature(float *temp)
{
    *temp = sensor_temp.read();
    printf("SYSTEM TEMP: %.3f",*temp);
}

int main() 
{
    int result = 0;    
    uint8_t buffer[1024];
    
    //Init the lwm2m structures
    lwm2m_context_t * lwm2mH = NULL;
    lwm2m_object_t * objArray[DEVICE_OBJ_NUM];
    lwm2m_security_t security;
    client_data_t data; 
    data.sock = udp;
    
    //Init the lwm2m server, set Leshan as default
    SocketAddress server(LESHAN_SERVER, LESHAN_PORT);
    SocketAddress client;

    pc.baud (115200);

    //Init display modual via LCD
    init_display();
    //Init the network modual via ethernet and udp socket
    init_network(); 
    
    /*
     * Now the main function fill an array with each object, this list will be later passed to liblwm2m.
     * Those functions are located in their respective object file.
     */
#ifdef SUPPORT_SECURITY
    char serverUri[50];
    int serverId = 101;
    char * pskId = NULL;
    uint16_t pskLen = -1;
    char * pskBuffer = NULL;

    sprintf (serverUri, "coap://%s:%s", LESHAN_SERVER, LESHAN_PORT_STR);

#ifdef LWM2M_BOOTSTRAP
    objArray[0] = get_security_object(serverId, serverUri, pskId, pskBuffer, pskLen, bootstrapRequested);
#else
    objArray[0] = get_security_object(serverId, serverUri, pskId, pskBuffer, pskLen, false);
#endif

    if (NULL == objArray[0])
    {
        fprintf(stderr, "Failed to create security object\r\n");
        return -1;
    }
    data.securityObjP = objArray[0];
    objArray[1] = get_server_object(serverId, "U", LWM2M_SERVER_TIMEOUT, false);
#else
    objArray[0] = get_security_object();
    if (NULL == objArray[0])
    {
        fprintf(stderr, "Failed to create security object\r\n");
        return -1;
    }
    data.securityObjP = objArray[0];

    objArray[1] = get_server_object();
#endif
    if (NULL == objArray[1])
    {
        fprintf(stderr, "Failed to create server object\r\n");
        return -1;
    }

    objArray[2] = get_object_device();
    if (NULL == objArray[2])
    {
        fprintf(stderr, "Failed to create Device object\r\n");
        return -1;
    }

    objArray[3] = get_object_temperature();
    if (NULL == objArray[3])
    {
        fprintf(stderr, "Failed to create temperature object\r\n");
        return -1;
    }
    
#ifdef ENABLE_PWM_RGB_LED
    objArray[4] = get_object_rgb_led();
    if (NULL == objArray[4])
    {
        fprintf(stderr, "Failed to create RGB LED object\r\n");
        return -1;
    }    
#endif
    /*
     * The liblwm2m library is now initialized with the functions that will be in
     * charge of communication
     */

    lwm2mH = lwm2m_init(&data);
    if (NULL == lwm2mH)
    {
        fprintf(stderr, "lwm2m_init() failed\r\n");
        return -1;
    }    

    printf("ontextP->endpointName %d , contextP->objectList %d\n", lwm2mH->endpointName, lwm2mH->objectList);
    /*
     * We configure the liblwm2m library with the name of the client - which shall be unique for each client -
     * the number of objects we will be passing through and the objects array
     */
    result = lwm2m_configure(lwm2mH, ENDPOINT_NAME, NULL, NULL, DEVICE_OBJ_NUM, objArray);
    if (result != 0)
    {
        fprintf(stderr, "lwm2m_configure() failed: 0x%X\r\n", result);
        return -1;
    }
    
    memset(&security, 0, sizeof(lwm2m_security_t));
    result = lwm2m_add_server(lwm2mH, 123, LWM2M_SERVER_TIMEOUT, NULL, BINDING_U, (void *)&server, &security);    
    if (result != 0)
    {
        fprintf(stderr, "lwm2m_add_server() failed: 0x%X\r\n", result);
        return -1;
    }   
    
    while (true) {
        int numBytes = 0;
        int recvTimeout =0;
        struct timeval tv;
        SocketAddress server1(LESHAN_SERVER, LESHAN_PORT);
        tv.tv_sec = LWM2M_CLIENT_TIMEOUT;
        tv.tv_usec = 0;    
        printf("\n=================================================================================\n"); 
        print_state(lwm2mH);
        printf("=================================================================================\n"); 
        /*
        * This function does two things:
        *  - first it does the work needed by liblwm2m (eg. (re)sending some packets).
        *  - Secondly it adjusts the timeout value (default 60s) depending on the state of the transaction
        *    (eg. retransmission) and the time before the next operation
        */
        result = lwm2m_step(lwm2mH, &(tv.tv_sec));
        if (result != 0)
        {
            printf("lwm2m_step() failed: 0x%x\r\n", result);
            return -1;
        }

        printf("\n---------------------------------------------------------------------------------\n");
        numBytes = udp.recvfrom(&server1, buffer, sizeof(buffer));
        if(numBytes <=0){
            printf("Error in recvfrom() - ret = %d\r\n",numBytes);
        }
        else
        {
            printf(">>>>>Received packet from: %s of size %d; session = 0x%x\n\n",server1.get_ip_address(), numBytes, (int)(&server1));
            //debug_dump((uint8_t *)buffer,numBytes);
            //Let liblwm2m respond to the query depending on the context
            lwm2m_handle_packet(lwm2mH, buffer, numBytes, (void*) &server);
        }
    }//while()

}//main


