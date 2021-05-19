// Unthreaded
unsigned char* tls_get_time(unsigned char client_random[32], char* anchor);

// Threaded
void threaded_tls_init(char* anchor, int runs);
unsigned char* threaded_tls_get_time(unsigned char client_random[32], char* anchor);
void threaded_tls_close();