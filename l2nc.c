#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/select.h>

#define ETHER_TYPE 0x88B5
#define BUF_SIZE 1514
#define ETH_HDR_LEN 14

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <interfaz>\n", argv[0]);
        return 1;
    }

    // 1. Crear socket RAW filtrando por el EtherType deseado
    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETHER_TYPE));
    if (sockfd == -1) {
        perror("Error al abrir socket (¿ejecutando como root?)");
        return 1;
    }

    // 2. Obtener índice de la interfaz
    struct ifreq ifr;
    strncpy(ifr.ifr_name, argv[1], IFNAMSIZ - 1);
    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) == -1) {
        perror("ioctl SIOCGIFINDEX");
        close(sockfd);
        return 1;
    }
    int ifindex = ifr.ifr_ifindex;

    // 3. Obtener la MAC de la interfaz local
    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) == -1) {
        perror("ioctl SIOCGIFHWADDR");
        close(sockfd);
        return 1;
    }

    // 4. Configurar estructura destino para el envío (Broadcast)
    struct sockaddr_ll sa;
    memset(&sa, 0, sizeof(sa));
    sa.sll_family = AF_PACKET;
    sa.sll_ifindex = ifindex;
    sa.sll_protocol = htons(ETHER_TYPE);
    sa.sll_halen = ETH_ALEN;
    memset(sa.sll_addr, 0xFF, ETH_ALEN); // MAC Destino: FF:FF:FF:FF:FF:FF

    // Buffer para transmisiones y recepciones
    unsigned char tx_buffer[BUF_SIZE];
    unsigned char rx_buffer[BUF_SIZE];

    // Pre-armar la cabecera Ethernet para enviar
    struct ether_header *eh = (struct ether_header *) tx_buffer;
    memcpy(eh->ether_shost, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
    memset(eh->ether_dhost, 0xFF, ETH_ALEN);
    eh->ether_type = htons(ETHER_TYPE);

    fd_set readfds;
    int maxfd = (sockfd > STDIN_FILENO) ? sockfd : STDIN_FILENO;

    // Bucle principal I/O Multiplexado
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sockfd, &readfds);

        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        // --- FLUJO 1: STDIN -> SOCKET (Lectura local y envío a red) ---
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            ssize_t bytes_read = read(STDIN_FILENO, 
                                      tx_buffer + ETH_HDR_LEN, 
                                      BUF_SIZE - ETH_HDR_LEN);
            
            if (bytes_read <= 0) {
                // EOF o Error en entrada (ej. Ctrl+D)
                break; 
            }

            // Enviar trama Ethernet
            ssize_t sent = sendto(sockfd, tx_buffer, bytes_read + ETH_HDR_LEN, 0,
                                  (struct sockaddr *)&sa, sizeof(sa));
            if (sent < 0) {
                perror("sendto");
                break;
            }
        }

        // --- FLUJO 2: SOCKET -> STDOUT (Recepción de red e impresión local) ---
        if (FD_ISSET(sockfd, &readfds)) {
            ssize_t bytes_recvd = recv(sockfd, rx_buffer, BUF_SIZE, 0);
            if (bytes_recvd < 0) {
                perror("recv");
                break;
            }

            // Validar que la trama contenga payload
            if (bytes_recvd > ETH_HDR_LEN) {
                // Escribir solo la carga útil ( payload ) a stdout
                write(STDOUT_FILENO, rx_buffer + ETH_HDR_LEN, bytes_recvd - ETH_HDR_LEN);
                fflush(stdout);
            }
        }
    }

    close(sockfd);
    return 0;
}
