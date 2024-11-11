#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h> /* needed for memset() */
#include <pthread.h>
#include <syslog.h>
#include <curl/curl.h>

#define MAX_PDU_SIZE         10000

#define PDU_CANDIDATE_LINE_OK	 1
#define PDU_ERROR_BAD_FORMAT	-1
#define PDU_NEED_MORE_DATA       0

#define ARRAY_SIZE 4

// #define DBG_RECEIVED_DATA
int processReceivedData(char *buffer, int buffersize, int *buffer_ptr, char *pdu_candidate, int *pdu_candidate_ptr);
int recv_mockup(int new_s, void *buffer, size_t len , int flags, char* read_simulation_payload);
// void *handle_connection(void *arg);
// char *assign_unique_id();
// char *perform_sentiment_analysis(const char *message);
// void log_message(const char *message, const char *sentiment);

int main(int argc, char* argv[])
{
  /* Not used but useful for pedagogical purposes */
  int new_s; // socket

  /* Read Buffer for TCP socket */
  char buffer[3000];
  /* number of input bytes read */
  int inbytes=-1;
  /* PDU candidate status */
  int pdu_status;
  /* Pointers used for PDU parsing */
  int buffer_ptr, pdu_candidate_ptr;
  /* Buffer used to store PDU candidate, it can be filled by several TCP segments */
  char pdu_candidate[MAX_PDU_SIZE+1];

  /* Initialization */
  pdu_candidate_ptr = 0;
  memset(pdu_candidate,0,sizeof(pdu_candidate));

  /* Buffer array for TCP read mockup */
  char *test_buffers[ARRAY_SIZE] = {
    "usuario1\x02timestamp1\x02mensaje1\x04",
    "usuario2\x02timestamp2\x02mensaje2\x04usuario3\x02timestamp3\x02mensaje3\x04",
    "usuario4\x02timestamp4\x02mensaje4\x04usuario5\x02timestamp5\x02mensaje5\x04usuario6\x02timestamp6\x02mensaje6\x04",
    "usuario7\x02timestamp7\x02mensaje7_incomplete"
  };

  /*  This for() emulates the reading loop */
  int i;
  for (i=0; i < ARRAY_SIZE; i++)
  {
    // Read mockup of a TCP socket new_s
    inbytes = recv_mockup(new_s, buffer, sizeof(buffer),0, test_buffers[i]);  //---> Aca sería donde se lee del socket (recv es de receive)
    printf("Read %s\n", buffer); // Just for debug, erase it if convenient
    buffer_ptr = 0; // after reading, buffer pointer is reseted to 0

    // String para guardar los campos de la PDU
    char PDU_data[1000];
    // This loop processes data and its existence provides
    // - complete and partial PDU parsing
    while (inbytes - buffer_ptr -1  > 0) // la cantidad de bytes que se leyeron menos la cantidad de bytes que ya se procesaron
    {
      // Mostramos lo que recibe la función processReceivedData
      // printf("buffer: %s\n", buffer);
      // printf("buffersize: %d\n", sizeof(buffer));
      // printf("len: %d\n", inbytes);
      // printf("inbytes: %d\n", inbytes);
      // printf("buffer_ptr: %d\n", buffer_ptr);
      // printf("pdu_candidate: %s\n", pdu_candidate);
      // printf("pdu_candidate_ptr: %d\n", pdu_candidate_ptr);

      pdu_status = processReceivedData((char *) buffer, inbytes, &buffer_ptr, (char *) pdu_candidate, &pdu_candidate_ptr);  // ---> Aca ve si hay un PDU completo, detecta los delimitadores y corta el buffer
      if (pdu_status == PDU_CANDIDATE_LINE_OK)  // ---> Aca se fija si el PDU es correcto
      {
        // Here we should try to parse the candidate PDU
        printf( "try_parse PDU\n");    // Just for debug, erase it if convenient
        printf("%s\n", pdu_candidate); // Just for debug, erase it if convenient
        
        // Parse PDU with ' ' as delimiter
        char *usuario = strtok(pdu_candidate, " ");
        char *timestamp = strtok(NULL, " ");
        char *mensaje = strtok(NULL, " ");

        // printf("Usuario: %s\n", usuario);
        // printf("Timestamp: %s\n", timestamp);
        // printf("Mensaje: %s\n", mensaje);
        // Guardar los campos de la PDU en un string
        sprintf(PDU_data, "Usuario: %s, Timestamp: %s, Mensaje: %s", usuario, timestamp, mensaje);
        printf("PDU_data: %s\n", PDU_data);
        
        // Memory cleaning
        pdu_candidate_ptr = 0;
        memset(pdu_candidate,0,sizeof(pdu_candidate));
      }
      else if (pdu_status == PDU_ERROR_BAD_FORMAT) // ---> caso en el que el PDU no es correcto
      {
        printf("ERROR clean memory buffer\n"); // Just for debug, erase it if convenient
        // Memory cleaning
        pdu_candidate_ptr = 0; 
        memset(pdu_candidate,0,sizeof(pdu_candidate));
      }
      else // ---> caso en el que no se detecta un delimitador, posiblemente porque el mensaje sigue en otro buffer
      {
        // Just for debug, erase it if convenient
        printf("%s\n", pdu_candidate);
        printf("No delimiter found. Probably need another buffer read\n");
      }
      // Just for debug, erase it if convenient
      printf("\n");
      fprintf(stderr,"processReceivedData = %d\n", pdu_status);
      fprintf(stderr,"inbytes=%d  buffer_ptr=%d\n", inbytes, buffer_ptr);
    }
  }

  return 0;
}

/*
 * This function frames the received data into candidate PDU
 * which will be tested for correcteness later on
 * This function was meant to delimit pipelined PDU (i.e. multiple PDU in one TCP segment/buffer)
 *
 * buffer: buffer which stores last read from socket // -> en nuestro ejemplo de 7 líneas, cada línea se cargaría en el buffer
 * buffersize: how many bytes (char) buffer holds
 * buffer_ptr: offset position at which it will read buffer "buffer"
 * pdu_candidate: buffer which stores byte hoping to have enough information to
 *   build a candidate PDU (find PDU delimiter)
 * pdu_candidate_ptr: offset position at which it will read buffer "pdu_candidate"
 */
int processReceivedData(char *buffer, int buffersize, int *buffer_ptr, char *pdu_candidate, int *pdu_candidate_ptr)
{ 
  // Position pdu_candidate pointer
  pdu_candidate += *pdu_candidate_ptr;
  // Position buffer pointer
  buffer += *buffer_ptr;

  int i;
  int chunk = (buffersize - *buffer_ptr);
  for (i=0; i < chunk; i++)
  {
    (*buffer_ptr)++; // must be before a return gets hit

    #ifdef DBG_RECEIVED_DATA
      if ( *buffer >= 32 && *buffer <=126 )
        fprintf(stderr,"Processing char (%d) '%c'\n", *buffer, *buffer);
      else
        fprintf(stderr,"Processing char (%d) \n", *buffer);
      if ( *pdu_candidate >= 32 && *pdu_candidate <=126 )
        fprintf(stderr,"pdu_candidate char (%d) '%c'\n", *pdu_candidate, *pdu_candidate);
      else
        fprintf(stderr,"pdu_candidate char (%d)\n", *pdu_candidate);
      fprintf(stderr,"buffer_ptr '%d'  pdu_candidate_ptr '%d'\n", *buffer_ptr, *pdu_candidate_ptr);
    #endif

    // PDU candidate criteria: \x04 (MSEP)
    if ( *pdu_candidate_ptr >= 1 && *buffer == 0x04)
    {
      pdu_candidate++; // Advance pointer for next PDU (in 2016 was not a requirement)
      *pdu_candidate++ = ' '; // Replace delimiter with space
      (*pdu_candidate_ptr)++;
      #ifdef DBG_RECEIVED_DATA
         fprintf(stderr,"MSEP detected PDU_CANDIDATE_LINE_OK\n");
      #endif
      return PDU_CANDIDATE_LINE_OK;
    }
    else if (*buffer !='\x02' && *buffer != '\x04' && (*buffer < 32 || *buffer >126) ) // http://www.asciitable.com/
    {
      // Forbidden characters
      // pdu_candidate_ptr must be taken care outside this function
      #ifdef DBG_RECEIVED_DATA
        fprintf(stderr,"Char outside of allowed range PDU_ERROR_BAD_FORMAT\n");
      #endif
      return PDU_ERROR_BAD_FORMAT;
    }
    else
    {
      #ifdef DBG_RECEIVED_DATA
      if ( *buffer >= 32 && *buffer <=126 )
        fprintf(stderr,"Char (%d) '%c' processed OK\n", *buffer, *buffer);
      else
        fprintf(stderr,"Char (%d) processed OK\n", *buffer);
      #endif
      if (*buffer == '\x02' || *buffer == '\x04') {
        *pdu_candidate = ' '; // Replace delimiter with space
      } else {
        *pdu_candidate = *buffer;
      }
      (*pdu_candidate_ptr)++;
      pdu_candidate++;
    }
    buffer++;
  }
  return PDU_NEED_MORE_DATA;
}


/*
 * This function emulates for pedagocial purposes recv() system call
 * it has an extra argument, which is what we want to simulate to receive
 */
int recv_mockup(int new_s, void *buffer, size_t len , int flags, char* read_simulation_payload)
{
  strcpy(buffer, read_simulation_payload);
  return strlen(buffer);
}

