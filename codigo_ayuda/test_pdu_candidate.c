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

// #define DBG_RECEIVED_DATA
int processReceivedData(char *buffer, int buffersize, int *buffer_ptr, char *pdu_candidate, int *pdu_candidate_ptr);
int recv_mockup(int new_s, void *buffer, size_t len , int flags, char* read_simulation_payload);
void *handle_connection(void *arg);
char *assign_unique_id();
char *perform_sentiment_analysis(const char *message);
void log_message(const char *message, const char *sentiment);

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
  char *test_buffers[7] = {
    "USER\x02TIMESTAMP\x02MESSAGE_BODY\x04",
    "USER\x02TIMESTAMP\x02MESSAGE_BODY\x04USER\x02TIMESTAMP\x02MESSAGE_BODY\x04",
    "USER\x02TIMESTAMP\x02MESSAGE_BODY\x04USER\x02TIMESTAMP\x02MESSAGE_BODY",
    "USER\x02TIMESTAMP\x02MESSAGE_BODY",
    "\x04USER\x02TIMESTAMP\x02MESSAGE_BODY\x04",
    "USER\x02TIMESTAMP\x02MESSAGE_BODY\x04",
    "USER\x02TIMESTAMP\x02MESSAGE_BODY\x04"
  };

  /*  This for() emulates the reading loop */
  int i;
  for (i=0; i < 7; i++)
  {
    // Read mockup of a TCP socket new_s
    inbytes = recv_mockup(new_s, buffer, sizeof(buffer),0, test_buffers[i]);
    buffer_ptr = 0; // after reading, buffer pointer is reseted to 0

    // This loop processes data and its existence provides
    // - complete and partial PDU parsing
    while (inbytes - buffer_ptr -1  > 0)
    {
      pdu_status = processReceivedData((char *) buffer, inbytes, &buffer_ptr, (char *) pdu_candidate, &pdu_candidate_ptr);
      if (pdu_status == PDU_CANDIDATE_LINE_OK)
      {
        // Here we should try to parse the candidate PDU
        printf( "try_parse PDU\n");    // Just for debug, erase it if convenient
        printf("%s\n", pdu_candidate); // Just for debug, erase it if convenient
        // try_parse((char *) pdu_candidate, pdu_candidate_ptr,  &new_s);
        // Memory cleaning
        pdu_candidate_ptr = 0;
        memset(pdu_candidate,0,sizeof(pdu_candidate));
      }
      else if (pdu_status == PDU_ERROR_BAD_FORMAT)
      {
        printf("ERROR clean memory buffer\n"); // Just for debug, erase it if convenient
        // Memory cleaning
        pdu_candidate_ptr = 0;
        memset(pdu_candidate,0,sizeof(pdu_candidate));
      }
      else
      {
        // Just for debug, erase it if convenient
        printf("%s\n", pdu_candidate);
        printf("No delimiter found. Probably need another buffer read\n");
      }
      // Just for debug, erase it if convenient
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
 * buffer: buffer which stores last read from socket
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

    // if ( *pdu_candidate_ptr >= 3 )
    //   printf("  %d-%d-%d-%d\n", *(pdu_candidate-2), *(pdu_candidate-1), *(pdu_candidate), *buffer );

    // PDU candidate criteria: \x04 (MSEP)
    if ( *pdu_candidate_ptr >= 1 && // We needed at least 1 char to test \x04 condition
         ( *(pdu_candidate) == '\x04' ) )
    {
      pdu_candidate++; // Advance pointer for next PDU (in 2016 was not a requirement)
      *pdu_candidate++ = *buffer;
      (*pdu_candidate_ptr)++;
      #ifdef DBG_RECEIVED_DATA
         fprintf(stderr,"MSEP detected PDU_CANDIDATE_LINE_OK\n");
      #endif
      return PDU_CANDIDATE_LINE_OK;
    }
    else if (*buffer != '\x02' &&  *pdu_candidate == '\x04')
    {
      // Forbidden character sequence '\x04' and not followed by '\x02'
      // pdu_candidate_ptr must be taken care outside this function
      #ifdef DBG_RECEIVED_DATA
         fprintf(stderr,"MSEP used alone without SEP PDU_ERROR_BAD_FORMAT\n");
      #endif
      // Go back pointer to avoid losing a real char
      (*buffer_ptr)--;
      return PDU_ERROR_BAD_FORMAT;
    }
    else if (*buffer == '\x02' &&  *pdu_candidate != '\x04')
    {
      // Forbidden character sequence '\x02' without preceeding '\x04'
      // pdu_candidate_ptr must be taken care outside this function
      #ifdef DBG_RECEIVED_DATA
         fprintf(stderr,"SEP detected but no previous MSEP PDU_ERROR_BAD_FORMAT\n");
      #endif
      return PDU_ERROR_BAD_FORMAT;
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
      if (*pdu_candidate != '\0') { pdu_candidate++; }
      *pdu_candidate = *buffer;
      (*pdu_candidate_ptr)++;
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

// Function to assign a unique identifier to each chat instance
char *assign_unique_id()
{
  static int id_counter = 0;
  char *id = malloc(20);
  sprintf(id, "chat-%04d", id_counter++);
  return id;
}

// Function to perform sentiment analysis on each message
char *perform_sentiment_analysis(const char *message)
{
  CURL *curl;
  CURLcode res;
  struct curl_slist *headers = NULL;
  char *sentiment = malloc(100);
  char post_data[256];

  curl = curl_easy_init();
  if(curl) {
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "X-API-Token: <token>");
    sprintf(post_data, "{\"message\": \"%s\"}", message);

    curl_easy_setopt(curl, CURLOPT_URL, "http://api.udesa.matsunaga.com.ar:15000/analyze");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);

    res = curl_easy_perform(curl);
    if(res != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
      strcpy(sentiment, "error");
    } else {
      // Parse the response to extract the sentiment
      // For simplicity, we assume the response is a plain string
      strcpy(sentiment, "positive"); // Replace with actual parsing logic
    }

    curl_easy_cleanup(curl);
  }

  return sentiment;
}

// Function to log each analyzed message using the syslog protocol
void log_message(const char *message, const char *sentiment)
{
  openlog("chat_app", LOG_PID|LOG_CONS, LOG_USER);
  syslog(LOG_INFO, "Message: %s, Sentiment: %s", message, sentiment);
  closelog();
}

// Function to handle each connection in a separate thread
void *handle_connection(void *arg)
{
  int new_s = *(int *)arg;
  free(arg);

  char buffer[3000];
  int inbytes = -1;
  int pdu_status;
  int buffer_ptr, pdu_candidate_ptr;
  char pdu_candidate[MAX_PDU_SIZE+1];

  pdu_candidate_ptr = 0;
  memset(pdu_candidate, 0, sizeof(pdu_candidate));

  char *test_buffers[7] = {
    "USER\x02TIMESTAMP\x02MESSAGE_BODY\x04",
    "USER\x02TIMESTAMP\x02MESSAGE_BODY\x04USER\x02TIMESTAMP\x02MESSAGE_BODY\x04",
    "USER\x02TIMESTAMP\x02MESSAGE_BODY\x04USER\x02TIMESTAMP\x02MESSAGE_BODY",
    "USER\x02TIMESTAMP\x02MESSAGE_BODY",
    "\x04USER\x02TIMESTAMP\x02MESSAGE_BODY\x04",
    "USER\x02TIMESTAMP\x02MESSAGE_BODY\x04",
    "USER\x02TIMESTAMP\x02MESSAGE_BODY\x04"
  };

  int i;
  for (i = 0; i < 7; i++)
  {
    inbytes = recv_mockup(new_s, buffer, sizeof(buffer), 0, test_buffers[i]);
    buffer_ptr = 0;

    while (inbytes - buffer_ptr - 1 > 0)
    {
      pdu_status = processReceivedData((char *)buffer, inbytes, &buffer_ptr, (char *)pdu_candidate, &pdu_candidate_ptr);
      if (pdu_status == PDU_CANDIDATE_LINE_OK)
      {
        printf("try_parse PDU\n");
        printf("%s\n", pdu_candidate);

        char *unique_id = assign_unique_id();
        char *sentiment = perform_sentiment_analysis(pdu_candidate);
        log_message(pdu_candidate, sentiment);

        free(unique_id);
        free(sentiment);

        pdu_candidate_ptr = 0;
        memset(pdu_candidate, 0, sizeof(pdu_candidate));
      }
      else if (pdu_status == PDU_ERROR_BAD_FORMAT)
      {
        printf("ERROR clean memory buffer\n");
        pdu_candidate_ptr = 0;
        memset(pdu_candidate, 0, sizeof(pdu_candidate));
      }
      else
      {
        printf("%s\n", pdu_candidate);
        printf("No delimiter found. Probably need another buffer read\n");
      }

      fprintf(stderr, "processReceivedData = %d\n", pdu_status);
      fprintf(stderr, "inbytes = %d  buffer_ptr = %d\n", inbytes, buffer_ptr);
    }
  }

  return NULL;
}
