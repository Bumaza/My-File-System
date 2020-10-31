#include "filesystem.h"
#include "util.h"

#include <string.h>
#include <stdio.h>

/* file_t info items */
#define FILE_T_OFFSET 0
#define FILE_T_SECTOR 1
#define FILE_T_SIZE 2
#define FILE_T_CURRENT 3

#define BIT_TABLE_SIZE 4
#define NUMBER_OF_BITS 8
#define BITS_IN_SECTOR 1024
#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
#define SET_TH_BIT(var, pos, value) (var ^= (-value ^ var) & (1UL << pos))


#define ROOT_SECTOR 4
#define ROOT_FOLDER 5

#define ROOT_SECTOR_DATA 31


#define MY_SECTOR_DATA 108
#define FILES_IN_FOLDER 26
#define AMOUNT_OF_DATA 124

typedef struct {
	char name[MAX_FILENAME];
	unsigned int size;
} zero_sector_t;

/**
* Bit table of sectors 
* size = hdd_size() / ((1 << 7) * (1 << 10))     // (2 ^ 7 * 2 ^ 10)
**/ 

/**
*  |--------2B------|-----4x4B-----|-------26x4B--------|
*  | pocet poloziek | next sectors |   files, folders   |
*
*  next sector - obsahuje sectory pouzitych files, folders
**/
typedef struct {
	unsigned int next_sector;
	unsigned int data[ROOT_SECTOR_DATA];
} my_root_sector;


/**
*  |---12B---|-----4B-----|-------112B-------|
*  |  name   | type, size |   data, sectors  |
*
* type - 2bit, size 30bit
**/
typedef struct {
	char name[MAX_FILENAME];
	unsigned int type_and_size;
	unsigned int next_sector; //data sector
	uint8_t data[MY_SECTOR_DATA];
} my_sector;

typedef struct {
	char name[MAX_FILENAME];
	unsigned int next_folder;
	unsigned int next_file_folder;
	unsigned int files[FILES_IN_FOLDER];
} folder_sector;

typedef struct {
	unsigned int next_sector;
	uint8_t data[AMOUNT_OF_DATA];
} data_sector;


/* help function for "debugging"*/
void fs_print(){

	uint8_t buffer[SECTOR_SIZE] = {0};
	uint8_t data_root_sector[SECTOR_SIZE] = {0};
	uint8_t root_sector[SECTOR_SIZE] = {0};


	unsigned int free_sectors = 0;
	unsigned int total_sectors = hdd_size() / SECTOR_SIZE;
	for(int table_sector= 0; table_sector < BIT_TABLE_SIZE && total_sectors > 0; table_sector++){
		hdd_read(table_sector, buffer);
		for(int s = 0; s < SECTOR_SIZE && total_sectors > 0; s++){
			for(int i = 0; i < NUMBER_OF_BITS && total_sectors > 0; i++, total_sectors--){
				if(!CHECK_BIT(buffer[s], i)){
					free_sectors++;
				}
			}
		}
	}

	printf("|-----------FILE SYSTEM------------|\n");
	printf("|       @uthor Jozef Budac         |\n");
	printf("|----------------------------------|\n");

	printf("\n\n| Number of free sectors: %d/%d\n", free_sectors, hdd_size()/SECTOR_SIZE);
	printf("| DISK size: %d\n", hdd_size());
	printf("|------------------------------------------------\n");

	printf("/root\n");


	hdd_read(ROOT_SECTOR, root_sector);
	for(int i = 0; i < ROOT_SECTOR_DATA; i++){
		if(((my_root_sector*)root_sector)->data[i] == 0) {
			continue;
		}

		hdd_read(((my_root_sector*)root_sector)->data[i], data_root_sector);
		unsigned int ts = ((my_sector*)data_root_sector)->type_and_size;
		int type = ts >> 31 | ts >> 30;
		printf("\t%s %d \t type: %s \t index: %d\n", ((my_sector*)data_root_sector)->name, ts, type > 2 ? "link" : type > 1 ? "folder" : "file", i);
		if(((my_sector*)data_root_sector)->type_and_size > 0){
			unsigned size = ((my_sector*)data_root_sector)->type_and_size < MY_SECTOR_DATA ? ((my_sector*)data_root_sector)->type_and_size : MY_SECTOR_DATA;
			for(int j = 0; j < MY_SECTOR_DATA; j++){
				printf("%c", ((my_sector*)data_root_sector)->data[j]);
			}
			int ss = 1;
			while(1){
				if(ss == 1 && ((my_sector*)data_root_sector)->next_sector == 0) break;
				// if(ss > 1 && ((data_sector*)data_root_sector)->next_sector == 0){
				// 	for(int j = 0; j < AMOUNT_OF_DATA; j++){
				// 		printf("%c", ((data_sector*)data_root_sector)->data[j]);
				// 	}
				// 	break;
				// }

				//uint8_t buf_data[SECTOR_SIZE] = {0};
				if(ss == 1) hdd_read(((my_sector*)data_root_sector)->next_sector, data_root_sector);
				
				for(int j = 0; j < AMOUNT_OF_DATA; j++){
					printf("%c", ((data_sector*)data_root_sector)->data[j]);
				}
				if(((data_sector*)data_root_sector)->next_sector == 0) break;

				hdd_read(((data_sector*)data_root_sector)->next_sector, data_root_sector);
				ss++;

			}
			
			printf("\n");
		}
	}


	unsigned int read_sector = ((my_root_sector*)root_sector)->next_sector;

	while(read_sector != 0){
		hdd_read(read_sector, buffer);
		for(int i = 0; i < FILES_IN_FOLDER; i++){
			uint8_t dalsie_data[SECTOR_SIZE] = {0};
			if(((folder_sector*)buffer)->files[i] != 0){
				hdd_read(((folder_sector*)buffer)->files[i], dalsie_data);
				unsigned int ts = ((my_sector*)dalsie_data)->type_and_size;
				int type = ts >> 31 | ts >> 30;
				printf("\t%s %d \t type: %s %d\n", ((my_sector*)dalsie_data)->name, ts, type > 2 ? "link" : type > 1 ? "folder" : "file");
			}
		}

		read_sector = ((folder_sector*)buffer)->next_file_folder;
	}

	printf("===================================================\n\n\n");
}

/* uvolni v BIT TABLE dany sektor*/
void free_sector(unsigned int sector){
	if(sector < 6) return;
	uint8_t table_sector[SECTOR_SIZE] = {0};
	for(int k = BIT_TABLE_SIZE-1; k >= 0; k--){
		if(sector > k * BITS_IN_SECTOR){ //je z tohoto sektora
			sector -= (k * BITS_IN_SECTOR);
			hdd_read(k, table_sector);
			for(int s = 0; s < SECTOR_SIZE; s++){
				for(int j = 0; j < NUMBER_OF_BITS; j++){
					if((j + s * NUMBER_OF_BITS) == sector){ 
						SET_TH_BIT(table_sector[s], j, 0);
						hdd_write(k, table_sector);
					}
				}
			}
		}
	}
}

/* vrati cislo volneo sektroa ak
 * ak neexistuje vrati -1 */
unsigned int get_sector(){
	uint8_t buffer[SECTOR_SIZE] = {0};
	unsigned int free_sector = -1;
	unsigned int total_sectors = hdd_size() / SECTOR_SIZE;
	for(int table_sector= 0; table_sector < BIT_TABLE_SIZE && free_sector == -1 && total_sectors > 0; table_sector++){
		hdd_read(table_sector, buffer);
		for(int s = 0; s < SECTOR_SIZE && free_sector == -1 && total_sectors > 0; s++){
			for(int i = 0; i < NUMBER_OF_BITS && total_sectors > 0; i++, total_sectors--){
				if(!CHECK_BIT(buffer[s], i)){ //nasli sme volny bit(SECTOR)
					free_sector = ((s * NUMBER_OF_BITS) + i + (table_sector * BITS_IN_SECTOR));
					SET_TH_BIT(buffer[s], i, 1);
					hdd_write(table_sector, buffer);
					return free_sector;
				}
			}
		}
	}
	return -1;
}



/**
 * Naformatovanie disku.
 *
 * Zavola sa vzdy, ked sa vytvara novy obraz disku. Mozete predpokladat, ze je
 * cely vynulovany.
 */

void fs_format()
{
	//prvych 6 sektorov je obsadnee
	//prve styri pre oznacenie sektroov to nam dava velkost disku 524288B
	//dalsi je pre root a dalsi pre root folders(tree)

	uint8_t obsadene[SECTOR_SIZE] = {0};
	obsadene[0] = 63;
	hdd_write(0, obsadene);
	uint8_t buffer[256] = { 0 };
	for(int i = 1; i <= ROOT_FOLDER; i++){
		hdd_write(i, buffer);
	}
}

/**
 * Vytvorenie suboru.
 *
 * Volanie vytvori v suborovom systeme na zadanej ceste novy subor a vrati
 * handle nan. Ak subor uz existoval, bude skrateny na 0. Pozicia v subore bude
 * nastavena na 0ty byte. Ak adresar, v ktorom subor ma byt ulozeny neexistuje,
 * vrati FAIL (sam nevytvara adresarovu strukturu, moze vytvarat iba subory).
 */

file_t *fs_creat(const char *path)
{
	uint8_t buffer[SECTOR_SIZE] = {0};
	uint8_t root_data_sector[SECTOR_SIZE] = {0};
	uint8_t root_sector[SECTOR_SIZE] = {0};
	uint8_t table_sector[SECTOR_SIZE] = {0};
	hdd_read(ROOT_SECTOR, root_sector);
	file_t *fd;

	unsigned int fr = -1;


	/* Je to adresar podme hladat v adresaroch*/
	if (strrchr(path, PATHSEP) != path){
		return (file_t*)FAIL;
	}

	/* Skontrolujeme, ci uz nahodou na disku nie je subor */
	for(int i = 0; i < ROOT_SECTOR_DATA; i++){
		//najskor v datach
		//printf("DATA BLOCK %d  -  %d\n", i, ((my_root_sector*)root_sector)->data[i]);
		if(((my_root_sector*)root_sector)->data[i] != 0){
			hdd_read(((my_root_sector*)root_sector)->data[i], root_data_sector);
			//printf("%s vs %s = %d\n", ((my_sector*)root_data_sector)->name, path, strcmp(((my_sector*)root_data_sector)->name, path));
			if(!strcmp(((my_sector*)root_data_sector)->name, path)){
				//musime skratit na nulu
				((my_sector*)root_data_sector)->type_and_size = 0;
				//premazat bloky
				if(((my_sector*)root_data_sector)->next_sector != 0){ //nie je nulovy nieco tam je 
					//uvolni tento sector v tabulke
					
					unsigned int sectors_to_free[hdd_size()/SECTOR_SIZE];
					sectors_to_free[0] = ((my_sector*)root_data_sector)->next_sector;
					((my_sector*)root_data_sector)->next_sector = 0; //vyjulujeme ho
					hdd_write(((my_root_sector*)root_sector)->data[i], root_data_sector);
					memset(buffer, 0, sizeof(buffer));
					int p = 1;
					while(1){ //uvolni aj "linkedliist"
						hdd_read(sectors_to_free[p-1], buffer);
						if(((data_sector*)buffer)->next_sector == 0) break;
						sectors_to_free[p++] = ((data_sector*)buffer)->next_sector;

					}
					for(int u = 0; u < p; u++){
						free_sector(sectors_to_free[u]);
					}
					((my_sector*)root_data_sector)->next_sector = 0;
				}

				hdd_write(((my_root_sector*)root_sector)->data[i], root_data_sector);
				fd = fd_alloc();
				fd->info[FILE_T_OFFSET] = 0;
				fd->info[FILE_T_SECTOR] = ((my_root_sector*)root_sector)->data[i];
				fd->info[FILE_T_SIZE] = 0;
				fd->info[FILE_T_CURRENT] = ((my_root_sector*)root_sector)->data[i];
				return fd;
			}
		}else{
			fr = i;
			break;
		}
	}
	/* Skontroluje aj ine bloky so subormi v roote*/
	if(fr == -1){
		memset(buffer, 0, sizeof(buffer));
		memset(root_data_sector, 0, sizeof(root_data_sector));
		if(((my_root_sector*)root_sector)->next_sector == 0){ //vytvor ho
			unsigned int free_sector = get_sector();
			unsigned int new_sector = get_sector();
			if(free_sector == -1 || new_sector == -1) return (file_t*)FAIL;
			((my_root_sector*)root_sector)->next_sector = free_sector;
			hdd_write(ROOT_SECTOR, root_sector);

			fd = fd_alloc();
			fd->info[FILE_T_OFFSET] = 0;
			fd->info[FILE_T_SIZE] = 0;
			fd->info[FILE_T_CURRENT] = new_sector;
			fd->info[FILE_T_SECTOR] = new_sector;

			((my_sector*)buffer)->type_and_size = 0;
			strncpy(((my_sector*)buffer)->name, path, MAX_FILENAME);
			hdd_write(new_sector, buffer);

			memset(buffer, 0, sizeof(buffer));
			((folder_sector*)buffer)->next_file_folder = 0;
			((folder_sector*)buffer)->files[0] = new_sector;
			hdd_write(free_sector, buffer);
			return fd;

		}else{
			unsigned int read_sector = ((my_root_sector*)root_sector)->next_sector;
			unsigned int prev_sector = 0;
			while(read_sector != 0){
				hdd_read(read_sector, buffer);
				for(int i = 0; i < FILES_IN_FOLDER; i++){
					if(((folder_sector*)buffer)->files[i] == 0){

						unsigned int new_sector = get_sector();
						if(new_sector == -1) return (file_t*)FAIL;

						((folder_sector*)buffer)->files[i] = new_sector;
						hdd_write(read_sector, buffer);

						uint8_t new_data[SECTOR_SIZE] = {0};
						((my_sector*)new_data)->type_and_size = 0;
						strncpy(((my_sector*)new_data)->name, path, MAX_FILENAME);
						hdd_write(new_sector, new_data);
						fd = fd_alloc();
						fd->info[FILE_T_OFFSET] = 0;
						fd->info[FILE_T_SIZE] = 0;
						fd->info[FILE_T_CURRENT] = new_sector;
						fd->info[FILE_T_SECTOR] = new_sector;

						return fd;
					}
				}
				prev_sector = read_sector;
				read_sector = ((folder_sector*)buffer)->next_file_folder;
			}
			//nenasiel miesto skus vytvorit dalsi root_folder

			unsigned int free_sector = get_sector();
			unsigned int new_sector = get_sector();
			if(free_sector == -1 || new_sector == -1) return (file_t*)FAIL;

			((folder_sector*)buffer)->next_file_folder = free_sector;
			hdd_write(prev_sector, buffer);

			fd = fd_alloc();
			fd->info[FILE_T_OFFSET] = 0;
			fd->info[FILE_T_SIZE] = 0;
			fd->info[FILE_T_CURRENT] = new_sector;
			fd->info[FILE_T_SECTOR] = new_sector;

			memset(buffer, 0 , sizeof(buffer));
			((my_sector*)buffer)->type_and_size = 0;
			strncpy(((my_sector*)buffer)->name, path, MAX_FILENAME);
			hdd_write(new_sector, buffer);

			memset(buffer, 0, sizeof(buffer));
			((folder_sector*)buffer)->next_file_folder = 0;
			((folder_sector*)buffer)->files[0] = new_sector;
			hdd_write(free_sector, buffer);
			return fd;

		}
	}

	/* Vytvarame novy pozrieme sa na volne sectory */
	//najdeme prvy volny sektor
	unsigned int free_sector = get_sector();

	//uz nemame ziaden volny sektor
	if(free_sector == -1) return (file_t*)FAIL;
	
	//free_sector += (ROOT_SECTOR + 1);

	uint8_t new_sector[SECTOR_SIZE] = {0};
	/* Vsetko ok */
	strncpy(((my_sector*)new_sector)->name, path, MAX_FILENAME);
	((my_sector*)new_sector)->type_and_size = 0;
	((my_root_sector*)root_sector)->data[fr] = free_sector;

	//printf("creating... %s\t size: %d \t on sector: %d\tin root %d is %d\n", ((my_sector*)new_sector)->name, ((my_sector*)new_sector)->type_and_size, free_sector, fr, ((my_root_sector*)root_sector)->data[fr]);

	/* Zapiseme informacie o novovytvorenom subore na disk */
	hdd_write(free_sector, new_sector);
	hdd_write(ROOT_SECTOR, root_sector);

	fd = fd_alloc();
	fd->info[FILE_T_OFFSET] = 0;
	fd->info[FILE_T_SECTOR] = free_sector;
	fd->info[FILE_T_SIZE] = 0;
	fd->info[FILE_T_CURRENT] = free_sector;
	return fd;
}


/**
 * Otvorenie existujuceho suboru.
 *
 * Ak zadany subor existuje funkcia ho otvori a vrati handle nan. Pozicia v
 * subore bude nastavena na 0ty bajt. Ak subor neexistuje, vrati FAIL. Struktura
 * file_t sa musi alokovat jedine pomocou fd_alloc.
 */
file_t *fs_open(const char *path)
{

	/* Je to adresar podme hladat v adresaroch*/
	if (strrchr(path, PATHSEP) != path){
		return (file_t*)FAIL;
	}

	file_t *fd;
	uint8_t data_sector[SECTOR_SIZE] = {0};
	uint8_t root_sector[SECTOR_SIZE] = {0};
	hdd_read(ROOT_SECTOR, root_sector);
	for(int i = 0; i < ROOT_SECTOR_DATA; i++){
		if(((my_root_sector*)root_sector)->data[i] == 0) continue;
		hdd_read(((my_root_sector*)root_sector)->data[i], data_sector);
		if(!strcmp(((my_sector*)data_sector)->name, path)){
			/* Subor existuje, alokujeme pren deskriptor */
			fd = fd_alloc();
			fd->info[FILE_T_OFFSET] = 0;
			fd->info[FILE_T_SECTOR] = ((my_root_sector*)root_sector)->data[i];
			fd->info[FILE_T_SIZE] = ((my_sector*)data_sector)->type_and_size;
			fd->info[FILE_T_CURRENT] = ((my_root_sector*)root_sector)->data[i];
			return fd;
		}
	}

	//others blocks
	uint8_t buffer[SECTOR_SIZE] = {0};
	uint8_t root_data_sector[SECTOR_SIZE] = {0};
	unsigned int read_sector = ((my_root_sector*)root_sector)->next_sector;
	while(read_sector > 0 && read_sector < hdd_size() / SECTOR_SIZE){
		hdd_read(read_sector, buffer);
		for(int i = 0; i < FILES_IN_FOLDER; i++){
			if(((folder_sector*)buffer)->files[i] == 0) continue;
			hdd_read(((folder_sector*)buffer)->files[i], root_data_sector);
			if(!strcmp(((my_sector*)root_data_sector)->name, path)){
				fd = fd_alloc();
				fd->info[FILE_T_OFFSET] = 0;
				fd->info[FILE_T_SECTOR] = ((folder_sector*)buffer)->files[i];
				fd->info[FILE_T_SIZE] = ((my_sector*)root_data_sector)->type_and_size;
				fd->info[FILE_T_CURRENT] = ((folder_sector*)buffer)->files[i];
				return fd;
			}
		}
		read_sector = ((folder_sector*)buffer)->next_file_folder;
	}

	return (file_t*)FAIL;
}

/**
 * Zatvori otvoreny file handle.
 *
 * Funkcia zatvori handle, ktory bol vytvoreny pomocou volania 'open' alebo
 * 'creat' a uvolni prostriedky, ktore su s nim spojene. V pripade akehokolvek
 * zlyhania vrati FAIL. Struktura file_t musi byt uvolnena jedine pomocou
 * fd_free.
 */
int fs_close(file_t *fd){
	fd_free(fd);
	return OK;
}

/**
 * Odstrani subor na ceste 'path'.
 *
 * Ak zadana cesta existuje a je to subor, odstrani subor z disku; nemeni
 * adresarovu strukturu. V pripade chyby vracia FAIL.
 */
int fs_unlink(const char *path){

	uint8_t buffer[SECTOR_SIZE] = {0};
	uint8_t data_root[SECTOR_SIZE] = {0};
	uint8_t root_sector[SECTOR_SIZE] = {0};

	/* Je to adresar podme hladat v adresaroch*/
	if (strrchr(path, PATHSEP) != path){
		//TODO
		return FAIL;
	}
	//nova cesta je root
	hdd_read(ROOT_SECTOR, root_sector);
	int i = 0;
	for(i = 0; i < ROOT_SECTOR_DATA; i++){
		if(((my_root_sector*)root_sector)->data[i] == 0) continue;
		hdd_read(((my_root_sector*)root_sector)->data[i], data_root);
		//printf("%s %s = \n", ((my_sector*)data_root)->name, path, strcmp(((my_sector*)data_root)->name, path));

		if(!strcmp(((my_sector*)data_root)->name, path)){

			unsigned int sectors_to_free[hdd_size()/SECTOR_SIZE];
			sectors_to_free[0] = ((my_root_sector*)root_sector)->data[i];
			((my_root_sector*)root_sector)->data[i] = 0; //vyjulujeme ho
			hdd_write(ROOT_SECTOR, root_sector);

			int p = 1;
			while(1){ //uvolni aj "linkedliist"
				hdd_read(sectors_to_free[p-1], buffer);
				if(p == 1){
					if(((my_sector*)buffer)->next_sector == 0) break;
					sectors_to_free[p++] = ((my_sector*)buffer)->next_sector;
					continue;
				}
				if(((data_sector*)buffer)->next_sector == 0) break;
				sectors_to_free[p++] = ((data_sector*)buffer)->next_sector;

			}

			for(int u = 0; u < p; u++){
				free_sector(sectors_to_free[u]);				
			}
			return OK;
		}
	}

	unsigned int read_sector = ((my_root_sector*)root_sector)->next_sector;
	while(read_sector != 0){
		hdd_read(read_sector, buffer);
		for(int i = 0; i < FILES_IN_FOLDER; i++){
			uint8_t dalsie_data[SECTOR_SIZE] = {0};
			if(((folder_sector*)buffer)->files[i] != 0){
				hdd_read(((folder_sector*)buffer)->files[i], dalsie_data);
				if(!strcmp(((my_sector*)dalsie_data)->name, path)){
					unsigned int sectors_to_free[hdd_size()/SECTOR_SIZE];
					sectors_to_free[0] = ((folder_sector*)buffer)->files[i];
					((folder_sector*)buffer)->files[i] = 0; //vyjulujeme ho
					hdd_write(read_sector, buffer);

					int p = 1;
					while(1){ //uvolni aj "linkedliist"
						hdd_read(sectors_to_free[p-1], buffer);
						if(p == 1){
							if(((my_sector*)buffer)->next_sector == 0) break;
							sectors_to_free[p++] = ((my_sector*)buffer)->next_sector;
							continue;
						}
						if(((data_sector*)buffer)->next_sector == 0) break;
						sectors_to_free[p++] = ((data_sector*)buffer)->next_sector;

					}

					for(int u = 0; u < p; u++){
						free_sector(sectors_to_free[u]);				
					}
					return OK;
				}
			}
		}
		read_sector = ((folder_sector*)buffer)->next_file_folder;
	}

	return FAIL;
}

/**
 * Premenuje/presunie polozku v suborovom systeme z 'oldpath' na 'newpath'.
 *
 * Po uspesnom vykonani tejto funkcie bude subor, ktory doteraz existoval na
 * 'oldpath' dostupny cez 'newpath' a 'oldpath' prestane existovat. Opat,
 * funkcia nemanipuluje s adresarovou strukturou (nevytvara nove adresare z cesty newpath okrem posledneho).
 * V pripade zlyhania vracia FAIL.
 */
int fs_rename(const char *oldpath, const char *newpath) {
	uint8_t buffer[SECTOR_SIZE] = {0};
	uint8_t root_data_sector[SECTOR_SIZE] = {0};
	uint8_t root_sector[SECTOR_SIZE] = {0};

	/* Je to adresar podme hladat v adresaroch*/
	if (strrchr(oldpath, PATHSEP) != oldpath){
		return FAIL;
	}

	//nova cesta je root
	hdd_read(ROOT_SECTOR, root_sector);
	int i = 0;
	for(i = 0; i < ROOT_SECTOR_DATA; i++){
		if(((my_root_sector*)root_sector)->data[i] == 0) continue;
		hdd_read(((my_root_sector*)root_sector)->data[i], root_data_sector);
		if(!strcmp(((my_sector*)root_data_sector)->name, oldpath)){
			strncpy(((my_sector*)root_data_sector)->name, newpath, MAX_FILENAME);
			hdd_write(((my_root_sector*)root_sector)->data[i], root_data_sector);
			return OK;
		}
	}


	unsigned int read_sector = ((my_root_sector*)root_sector)->next_sector;
	while(read_sector > 0){
		hdd_read(read_sector, buffer);
		for(int i = 0; i < FILES_IN_FOLDER; i++){
			if(((folder_sector*)buffer)->files[i] == 0) continue;
			hdd_read(((folder_sector*)buffer)->files[i], root_data_sector);
			if(!strcmp(((my_sector*)root_data_sector)->name, oldpath)){
				strncpy(((my_sector*)root_data_sector)->name, newpath, MAX_FILENAME);
				hdd_write(((folder_sector*)buffer)->files[i], root_data_sector);
				return OK;
			}
		}
		read_sector = ((folder_sector*)buffer)->next_file_folder;
	}

	//TODO check other root sector fort file

	return FAIL;

}

/**
 * Nacita z aktualnej pozicie vo 'fd' do bufferu 'bytes' najviac 'size' bajtov.
 *
 * Z aktualnej pozicie v subore precita funkcia najviac 'size' bajtov; na konci
 * suboru funkcia vracia 0. Po nacitani dat zodpovedajuco upravi poziciu v
 * subore. Vrati pocet precitanych bajtov z 'bytes', alebo FAIL v pripade
 * zlyhania. Existujuci subor prepise.
 */
int fs_read(file_t *fd, uint8_t *bytes, unsigned int size)
{
	/* Podporujeme iba subory s maximalnou velkostou SECTOR_SIZE */
	uint8_t buffer[SECTOR_SIZE] = {0};
	unsigned int offset = fd->info[FILE_T_OFFSET];
	unsigned int file_size = fd->info[FILE_T_SIZE];

	/* Nacitame celkovu velkost suboru na disku */
	hdd_read(fd->info[FILE_T_CURRENT], buffer);

	//if(file_size < offset + size) return FAIL;
	unsigned int ms = MY_SECTOR_DATA;
	unsigned int pridaj = 0;

	unsigned int read_sector = 0;
	int i = 0;
	if(fd->info[FILE_T_CURRENT] == fd->info[FILE_T_SECTOR]){
		for (i = 0; (i < size) && ((i + offset) < MY_SECTOR_DATA); i++) {
			bytes[i] = ((my_sector*)buffer)->data[offset + i];
		}
		read_sector = ((my_sector*)buffer)->next_sector;
		offset = 0;
	}else{
		pridaj = MY_SECTOR_DATA;
		read_sector = fd->info[FILE_T_CURRENT];

		//dopocitaj sa do aktualneho kolko sektorov aby sme 
		//vedeli celkovu size(nevieme kde sme v kotrom sektroe)
		hdd_read(fd->info[FILE_T_SECTOR], buffer);
		uint8_t my_buff[SECTOR_SIZE] = {0};

		unsigned count_sector = ((my_sector*)buffer)->next_sector;
		while(count_sector != fd->info[FILE_T_CURRENT]){
			ms += AMOUNT_OF_DATA;
			pridaj+= AMOUNT_OF_DATA;
			hdd_read(count_sector, my_buff);
			count_sector = ((data_sector*)my_buff)->next_sector;
		}
		ms += offset;
	}
	unsigned int N = (size - i); // treba precitat este N bajtov
	uint8_t data_s[SECTOR_SIZE] = {0};

	while(N > 0 && read_sector > 0){
		hdd_read(read_sector, data_s);
		for(int j = 0; j + offset < AMOUNT_OF_DATA && i < size 
				&& i+fd->info[FILE_T_OFFSET] < file_size
				&& ms < file_size; j++, i++, ms++){
			bytes[i] = ((data_sector*)data_s)->data[j + offset];
		}
		offset = 0;
		read_sector = ((data_sector*)data_s)->next_sector;
	}
	
	/* Aktualizujeme offset, na ktorom sme */
	//fd->info[FILE_T_OFFSET] += size;
	fs_seek(fd, fd->info[FILE_T_OFFSET] + i + pridaj);


	/* Vratime pocet precitanych bajtov */
	return i;
}

/**
 * Zapise do 'fd' na aktualnu poziciu 'size' bajtov z 'bytes'.
 *
 * Na aktualnu poziciu v subore zapise 'size' bajtov z 'bytes'. Ak zapis
 * presahuje hranice suboru, subor sa zvacsi; ak to nie je mozne, zapise sa
 * maximalny mozny pocet bajtov. Po zapise korektne upravi aktualnu poziciu v
 * subore a vracia pocet zapisanych bajtov z 'bytes'.
 */

int fs_write(file_t *fd, const uint8_t *bytes, unsigned int size)
{
	uint8_t buffer[SECTOR_SIZE] = { 0 };
	uint8_t table[SECTOR_SIZE] = {0};
	/* Vo filedescriptore je ulozena nasa aktualna pozicia v subore */
	unsigned int offset = fd->info[FILE_T_OFFSET];
	unsigned int file_size = fd->info[FILE_T_SIZE];


	/* Nacitame stare data do buffera a prepiseme ich novymi */
	hdd_read(fd->info[FILE_T_CURRENT], buffer);

	int i;
	if(fd->info[FILE_T_CURRENT] == fd->info[FILE_T_SECTOR]){//sme v zakladnom sektore
		for (i = 0; (i < size) && ((i + offset) < MY_SECTOR_DATA); i++) {
			((my_sector*)buffer)->data[offset + i] = bytes[i];
		}
	}

	unsigned int N = (size - i); // treba zapisat este N bajtov
	uint8_t data_s[SECTOR_SIZE] = {0};

	while(N > 0){ 
			//najdi volny sektor
		unsigned int fr = get_sector();
			//uz nemame ziaden volny sektor
		if(fr == -1){

			fd->info[FILE_T_SIZE] = (size - N);

			hdd_read(fd->info[FILE_T_SECTOR], buffer);
			((my_sector*)buffer)->type_and_size = size - N;
			hdd_write(fd->info[FILE_T_SECTOR], buffer);
			fd->info[FILE_T_OFFSET] += (size - N);

			return (size - N);
		}

			//zapis 
		if(fd->info[FILE_T_CURRENT] == fd->info[FILE_T_SECTOR]){
			((my_sector*)buffer)->next_sector = fr;
				//printf("MY NAME IS %s\n", ((my_sector*)buffer)->name);
			hdd_write(fd->info[FILE_T_SECTOR], buffer);
		}else{
			((data_sector*)data_s)->next_sector = fr;
			hdd_write(fd->info[FILE_T_CURRENT], data_s);
		}
		fd->info[FILE_T_CURRENT] = fr;

		memset(data_s, 0, sizeof(data_s));

		int j;
		for (j = 0; (j < N) && (j < AMOUNT_OF_DATA); j++) {
			((data_sector*)data_s)->data[j] = bytes[i++];
		}

		hdd_write(fr, data_s);

		N -= j;
	}

	/* Ak subor narastol, aktualizujeme velkost */
	if (file_size < offset + size) {
		fd->info[FILE_T_SIZE] = offset + size;
		((my_sector*)buffer)->type_and_size = offset + size;
	}
	hdd_write(fd->info[FILE_T_SECTOR], buffer);

	/* Aktualizujeme offset, na ktorom sme */
	fd->info[FILE_T_OFFSET] += size;

	/* Vratime pocet precitanych bajtov */
	return size;
}

/**
 * Zmeni aktualnu poziciu v subore na 'pos'-ty byte.
 *
 * Upravi aktualnu poziciu; ak je 'pos' mimo hranic suboru, vrati FAIL a pozicia
 * sa nezmeni, inac vracia OK.
 */


int fs_seek(file_t *fd, unsigned int pos)
{
	uint8_t buffer[SECTOR_SIZE] = { 0 };
	unsigned int file_size;

	file_size = fd->info[FILE_T_SIZE];

	/* Nemozeme seekovat za velkost suboru */
	if (pos > file_size) {
		fprintf(stderr, "Can not seek: %d > %d\n", pos, file_size);
		return FAIL;
	}

	if(pos < MY_SECTOR_DATA){
		fd->info[FILE_T_OFFSET] = pos;
		fd->info[FILE_T_CURRENT] = fd->info[FILE_T_SECTOR];
		return OK;
	}
	pos -= MY_SECTOR_DATA;

	unsigned int sector;
	unsigned int kolky = pos / AMOUNT_OF_DATA;

	hdd_read(fd->info[FILE_T_SECTOR], buffer);
	sector = ((my_sector*)buffer)->next_sector;

	for(int i = 0; i < kolky; i++){
		if(sector == 0) return FAIL;
		hdd_read(sector, buffer);
		sector = ((data_sector*)buffer)->next_sector;
	}

	fd->info[FILE_T_CURRENT] = sector;
	fd->info[FILE_T_OFFSET] = pos % AMOUNT_OF_DATA;

	return OK;
}


/**
 * Vrati aktualnu poziciu v subore.
 */

unsigned int fs_tell(file_t *fd) {
	return fd->info[FILE_T_OFFSET];
}


/**
 * Vrati informacie o 'path'.
 *
 * Funkcia vrati FAIL ak cesta neexistuje, alebo vyplni v strukture 'fs_stat'
 * polozky a vrati OK:
 *  - st_size: velkost suboru v byte-och
 *  - st_nlink: pocet hardlinkov na subor (ak neimplementujete hardlinky, tak 1)
 *  - st_type: hodnota podla makier v hlavickovom subore: ST_TYPE_FILE,
 *  ST_TYPE_DIR, ST_TYPE_SYMLINK
 *
 */

int fs_stat(const char *path, struct fs_stat *fs_stat) { 
	
	uint8_t buffer[SECTOR_SIZE] = {0};
	uint8_t root_data_sector[SECTOR_SIZE] = {0};
	uint8_t root_sector[SECTOR_SIZE] = {0};

	/* Je to adresar podme hladat v adresaroch*/
	if (strrchr(path, PATHSEP) != path){
		return FAIL;
	}

	//nova cesta je root
	hdd_read(ROOT_SECTOR, root_sector);
	int i = 0;
	for(i = 0; i < ROOT_SECTOR_DATA; i++){
		if(((my_root_sector*)root_sector)->data[i] == 0) continue;
		hdd_read(((my_root_sector*)root_sector)->data[i], root_data_sector);

		if(!strcmp(((my_sector*)root_data_sector)->name, path)){
			fs_stat->st_size = ((my_sector*)root_data_sector)->type_and_size;
			fs_stat->st_nlink = 1;
			fs_stat->st_type = ST_TYPE_FILE;
			return OK;
		}
	}

	unsigned int read_sector = ((my_root_sector*)root_sector)->next_sector;
	while(read_sector > 0){
		hdd_read(read_sector, buffer);
		for(int i = 0; i < FILES_IN_FOLDER; i++){
			if(((folder_sector*)buffer)->files[i] == 0) continue;
			hdd_read(((folder_sector*)buffer)->files[i],root_data_sector);
			if(!strcmp(((my_sector*)root_data_sector)->name, path)){
				fs_stat->st_size = ((my_sector*)root_data_sector)->type_and_size;
				fs_stat->st_nlink = 1;
				fs_stat->st_type = ST_TYPE_FILE;
				return OK;
			}
		}
		read_sector = ((folder_sector*)buffer)->next_file_folder;
	}


	return FAIL; 
};

/* Level 3 */
/**
 * Vytvori adresar 'path'.
 *
 * Ak cesta, v ktorej adresar ma byt, neexistuje, vrati FAIL (vytvara najviac
 * jeden adresar), pri korektnom vytvoreni OK.
 */
int fs_mkdir(const char *path) { 

    //TODO, check
	 unsigned int tree_number = 0;
	 for(int i = 0; i < strlen(path); i++){
	 	if(path[i] == PATHSEP) tree_number++;
	 }

	 uint8_t f_buffer[SECTOR_SIZE] = {0};
	 hdd_read(ROOT_FOLDER, f_buffer);

	 for(int i =0; i < tree_number; i++){
         char folder_name[MAX_FILENAME];
         int poc = -1;int k = 0;
         for(int j = 0; j < strlen(path); j++){
             if(path[j] =='/') poc++;
             if(poc == i) folder_name[k++] = path[j];
             if(poc > i) break;
         }
         folder_name[k]='\0';
         printf("%s\n", folder_name);

         if(i == tree_number-1){//vytvaras vsetko je ok ak si sa dostal az sem
         	//kazdy
         	//musime najst volne miesto na alokaciu
         	for(int u = 0; u < AMOUNT_OF_DATA; u++){
         		unsigned int node = ((data_sector*)f_buffer)->data[u];
         		if(node == 0){

         		}

         	}

         }

         //hladas
         unsigned int nasiel = 0;
         while(1){
         	for(int u = 0; u < AMOUNT_OF_DATA; u++){
	         	uint8_t sub_buffer[SECTOR_SIZE] = {0};
	         	unsigned int node = ((data_sector*)f_buffer)->data[u];
	         	if(node == 0) continue;
	         	hdd_read(node, sub_buffer);
	         	if(!strcmp(((folder_sector*)sub_buffer)->name, folder_name)){
	         		nasiel = node;
	         		hdd_read(((folder_sector*)sub_buffer)->next_file_folder, f_buffer);
	         	}
         	}
         	if(nasiel) break;
         	else if(!nasiel && ((data_sector*)f_buffer)->next_sector != 0){
         		hdd_read(((data_sector*)f_buffer)->next_sector, f_buffer);
         	}else if(!nasiel && ((data_sector*)f_buffer)->next_sector == 0){
         		return FAIL;
         	}
         }
     }

	return FAIL; 
}

/**
 * Odstrani adresar 'path'.
 *
 * Odstrani adresar, na ktory ukazuje 'path'; ak neexistuje alebo nie je
 * adresar, vrati FAIL; po uspesnom dokonceni vrati OK.
 */
int fs_rmdir(const char *path) {

return FAIL;
};

/**
 * Otvori adresar 'path' (na citanie poloziek)
 *
 * Vrati handle na otvoreny adresar s poziciou nastavenou na 0; alebo FAIL v
 * pripade zlyhania.
 */
file_t *fs_opendir(const char *path) { return (file_t*)FAIL; };

/**
 * Nacita nazov dalsej polozky z adresara.
 *
 * Do dodaneho buffera ulozi nazov polozky v adresari a posunie aktualnu
 * poziciu na dalsiu polozku. V pripade problemu, alebo nemoznosti precitat
 * polozku (dalsia neexistuje) vracia FAIL.
 */
int fs_readdir(file_t *dir, char *item) {return FAIL; };

/** 
 * Zatvori otvoreny adresar.
 */
int fs_closedir(file_t *dir) { return FAIL; };

/* Level 4 */
/**
 * Vytvori hardlink zo suboru 'path' na 'linkpath'.
 */
int fs_link(const char *path, const char *linkpath) { return FAIL; };

/**
 * Vytvori symlink z 'path' na 'linkpath'.
 */
int fs_symlink(const char *path, const char *linkpath) { return FAIL; };

