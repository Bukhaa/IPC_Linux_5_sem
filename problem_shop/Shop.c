#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>

#define COUNT_SHOP 5
#define COUNT_VISITOR 3
#define MIN_COUNT_PRODUCT 950
#define MAX_COUNT_PRODUCT 1050
#define MIN_BUY 4500
#define MAX_BUY 5500
#define LOADER 500

typedef struct visitor{
	
	int id;
	int potrebnost;
}visitor;

pthread_mutex_t mag_mutex[COUNT_SHOP];
int shops[COUNT_SHOP];
visitor v[COUNT_VISITOR];
int count_leave_visitors = 0;


void* GoShop(void *arg){
	
	visitor* v = (visitor*) arg;
	int current_shop;
	printf("У клиента %d потребность %d\n", v->id, v->potrebnost);
	
	while(v->potrebnost > 0){
		
		current_shop = rand() % COUNT_SHOP;
		pthread_mutex_lock(&mag_mutex[current_shop]);
		
		if(shops[current_shop] == 0){
			
			printf("Клиент %d зашел в пустой магазин %d\n", v->id, current_shop);
			sleep(2);
		}
		
		else{
			
			if(v->potrebnost - shops[current_shop] >= 0){
				
				v->potrebnost -= shops[current_shop];
				printf("Клиент %d опустошил %d магазин в котором было %d. У него осталось потребности: %d\n",v->id, current_shop, 
				shops[current_shop], v->potrebnost);
				shops[current_shop] = 0;
			}
			
			else{
				
				v->potrebnost = 0;
				shops[current_shop] -= v->potrebnost;
				printf("У клиента %d закончились деньги после покупки в магазине %d\n", v->id, current_shop);
			}
			
		}
		
		pthread_mutex_unlock(&mag_mutex[current_shop]);
        sleep(3);
	}
	
	count_leave_visitors++;
	pthread_exit(NULL);
	
}

void* LoadProduct(void *arg){
	
	int current_shop;
	while(count_leave_visitors < COUNT_VISITOR){
		
		current_shop = rand() % COUNT_SHOP;
		pthread_mutex_lock(&mag_mutex[current_shop]);
		shops[current_shop]+= LOADER;
		printf("Загрузчик заполнил магазин %d\n", current_shop);
		pthread_mutex_unlock(&mag_mutex[current_shop]);
		sleep(1);
	}
	
	pthread_exit(NULL);
}

int main(){
	
	setlocale(LC_ALL,"rus");
	srand(time(NULL));
	int* status;
	int res_create;
	int res_join;

	for(size_t i = 0; i < COUNT_SHOP; ++i){
		
		pthread_mutex_init(&mag_mutex[i], NULL);
		shops[i] = rand() % (MAX_COUNT_PRODUCT - MIN_COUNT_PRODUCT) + MIN_COUNT_PRODUCT;
	}
	
	for(size_t j = 0; j < COUNT_VISITOR; ++j) {
		
		v[j].potrebnost = rand() % (MAX_BUY - MIN_BUY) + MIN_BUY;
		v[j].id = j;
	}
	
	pthread_t cust[COUNT_VISITOR], loader;
    for (size_t i = 0; i < COUNT_VISITOR; ++i) {
        if((res_create = pthread_create(&cust[i], NULL, GoShop, (void*)&v[i])) != 0){
			printf("error create thread, status = %d\n", res_create);
			exit(EXIT_FAILURE);
        }
    }
	
    res_create = pthread_create(&loader, NULL, LoadProduct, NULL);
	if(res_create != 0){
		printf("error create thread, status = %d\n", res_create);
		exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < COUNT_VISITOR; ++i){
        if((res_join = pthread_join(cust[i], (void **) &status)) != 0){
			printf("error join thread, status = %d\n", res_join);
			exit(EXIT_FAILURE);
        }
    }
    res_join = pthread_join(loader, (void **) &status);
	if(res_join != 0){
		printf("error join thread, status = %d\n", res_join);
		exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < COUNT_SHOP; ++i) {
        pthread_mutex_destroy(&mag_mutex[i]);
    }
	 
	exit(EXIT_SUCCESS);
}









