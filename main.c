/**********************************************************************
 *  Problema Produtor–Consumidor usando semáforo “manual” em C / POSIX
 *  Autor........: (seu nome aqui)
 *  Licença......: MIT
 *********************************************************************/

 #include <pthread.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include <unistd.h>     /* usleep */
 #include <time.h>       /* time   */
 
 /* ------------------------------ Constantes ------------------------------ */
 #define TAM_BUFFER      10      /* capacidade do buffer circular */
 #define ATRASO_PROD_US  50000   /* 50 ms – deixa a saída legível   */
 #define ATRASO_CONS_US  80000   /* 80 ms                           */
 
 /* ------------------------ Estrutura do semáforo ------------------------- */
 typedef struct {
     int valor;                  /* contador (>= 0)                */
     pthread_mutex_t mutex;      /* protege 'valor'                */
     pthread_cond_t  cond;       /* bloqueia / acorda as threads   */
 } semaforo_t;
 
 /* inicializa o semáforo com o valor 'v' */
 void semaforo_init(semaforo_t *s, int v) {
     s->valor = v;
     pthread_mutex_init(&s->mutex, NULL);
     pthread_cond_init (&s->cond , NULL);
 }
 
 /* down / P / wait – bloqueia se valor == 0                      */
 void semaforo_down(semaforo_t *s) {
     pthread_mutex_lock(&s->mutex);
     while (s->valor == 0)
         pthread_cond_wait(&s->cond, &s->mutex);
     s->valor--;
     pthread_mutex_unlock(&s->mutex);
 }
 
 /* up / V / signal – libera quem estiver esperando                */
 void semaforo_up(semaforo_t *s) {
     pthread_mutex_lock(&s->mutex);
     s->valor++;
     pthread_cond_signal(&s->cond);
     pthread_mutex_unlock(&s->mutex);
 }
 
 /* ------------------------- Estruturas globais --------------------------- */
 typedef struct {
     int dados[TAM_BUFFER];
     int in;                      /* próxima posição livre    */
     int out;                     /* próxima posição ocupada  */
     pthread_mutex_t mutex_buf;   /* exclusão mútua no buffer */
     semaforo_t cheio;            /* quantos slots ocupados   */
     semaforo_t vazio;            /* quantos slots livres     */
 } buffer_t;
 
 buffer_t buffer;
 
 /* parâmetros de execução passados na linha de comando */
 int qt_produtores, qt_consumidores, itens_por_produtor;
 
 /* ---------------------- Operações sobre o buffer ------------------------ */
 void buffer_init(buffer_t *b) {
     b->in  = b->out = 0;
     pthread_mutex_init(&b->mutex_buf, NULL);
     semaforo_init(&b->cheio, 0);               /* começa vazio  */
     semaforo_init(&b->vazio, TAM_BUFFER);      /* N posições    */
 }
 
 /* insere um item – chamada pelo produtor */
 void inserir_item(buffer_t *b, int item) {
     semaforo_down(&b->vazio);                  /* aguarda vaga          */
     pthread_mutex_lock(&b->mutex_buf);         /* seção crítica          */
 
     b->dados[b->in] = item;
     b->in = (b->in + 1) % TAM_BUFFER;
 
     pthread_mutex_unlock(&b->mutex_buf);
     semaforo_up(&b->cheio);                    /* sinaliza item presente */
 }
 
 /* remove um item – chamada pelo consumidor; retorna o item obtido */
 int remover_item(buffer_t *b) {
     semaforo_down(&b->cheio);                  /* aguarda item           */
     pthread_mutex_lock(&b->mutex_buf);         /* seção crítica          */
 
     int item = b->dados[b->out];
     b->out = (b->out + 1) % TAM_BUFFER;
 
     pthread_mutex_unlock(&b->mutex_buf);
     semaforo_up(&b->vazio);                    /* sinaliza vaga livre    */
     return item;
 }
 
 /* ---------------------------- Rotinas de thread ------------------------- */
 void *rotina_produtor(void *arg) {
     long id = (long) arg;
     for (int i = 0; i < itens_por_produtor; i++) {
         int item = rand() % 1000;              /* “produz” algo          */
         inserir_item(&buffer, item);
         printf("[Produtor %ld] produziu %d\n", id, item);
         usleep(ATRASO_PROD_US);
     }
     return NULL;
 }
 
 void *rotina_consumidor(void *arg) {
     long id = (long) arg;
     /* consumidores rodam até serem cancelados pelo main */
     while (1) {
         int item = remover_item(&buffer);
         printf("        [Consumidor %ld] consumiu %d\n", id, item);
         usleep(ATRASO_CONS_US);
     }
     return NULL;
 }
 
 /* ------------------------------ Função main ----------------------------- */
 int main(int argc, char *argv[]) {
     if (argc != 4) {
         fprintf(stderr,
             "Uso: %s <produtores> <consumidores> <itens_por_produtor>\n", argv[0]);
         exit(EXIT_FAILURE);
     }
     qt_produtores      = atoi(argv[1]);
     qt_consumidores    = atoi(argv[2]);
     itens_por_produtor = atoi(argv[3]);
 
     if (qt_produtores<=0 || qt_consumidores<=0 || itens_por_produtor<=0) {
         fprintf(stderr, "Todos os parâmetros devem ser inteiros positivos.\n");
         exit(EXIT_FAILURE);
     }
 
     buffer_init(&buffer);
     srand(time(NULL));
 
     pthread_t th_prod[qt_produtores], th_cons[qt_consumidores];
 
     /* cria threads produtoras */
     for (long i = 0; i < qt_produtores; i++)
         pthread_create(&th_prod[i], NULL, rotina_produtor, (void*) i);
 
     /* cria threads consumidoras */
     for (long i = 0; i < qt_consumidores; i++)
         pthread_create(&th_cons[i], NULL, rotina_consumidor, (void*) i);
 
     /* aguarda término dos produtores */
     for (int i = 0; i < qt_produtores; i++)
         pthread_join(th_prod[i], NULL);
 
     /* todos os itens já foram produzidos – encerramos consumidores         */
     for (int i = 0; i < qt_consumidores; i++)
         pthread_cancel(th_cons[i]), pthread_join(th_cons[i], NULL);
 
     puts(">>> Execução concluída com sucesso.");
     return 0;
 }