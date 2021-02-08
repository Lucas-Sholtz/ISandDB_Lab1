#pragma warning(disable:4996)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MASTERINDEXFILE "master.ind"
#define MASTERDATAFILE "master.fl"
#define MASTERGARBAGEFILE "master_garbage.txt"
#define INDEXSIZE sizeof(struct Index)
#define MASTERSIZE sizeof(struct Master)
#define SLAVEDATAFILE "slave.fl"
#define SLAVEDATAFILE "slave_garbage.txt"
#define SLAVE_SIZE sizeof(struct Slave)

#pragma region Structures
struct Index
{
	int id;
	int address;
	int status;
};

struct Master
{
	int id;
	char name[16];
	int status;
	int slavesQuantity;
	long firstSlaveAddr;
};
struct Slave
{
	int masterId;
	int productId;
	int price;
	int amount;
	int exists;
	long selfAddress;
	long nextAddress;
};
#pragma endregion
#pragma region Test
int doesFileExists(FILE* indexTable, FILE* database, char* error)
{
	if (indexTable == NULL || database == NULL)				// Файли БД ще не існують
	{
		strcpy(error, "database files are not created yet");
		return 0;
	}

	return 1;
}

int doesIndexExists(FILE* indexTable, char* error, int id)
{
	fseek(indexTable, 0, SEEK_END);

	long indexTableSize = ftell(indexTable);

	if (indexTableSize == 0 || id * sizeof(struct Index) > indexTableSize)
	{
		strcpy(error, "no such ID in table");				// Такого номеру в табличці нема
		return 0;
	}

	return 1;
}

int doesRegExists(struct Index indexer, char* error)
{
	if (!indexer.status)									// Запис було вилучено
	{
		strcpy(error, "the record you\'re looking for has been removed");
		return 0;
	}

	return 1;
}

int doesPairUnique(struct Master master, int productId)
{
	FILE* slavesDb = fopen(SLAVEDATAFILE, "r+b");
	struct Slave slave;

	fseek(slavesDb, master.firstSlaveAddr, SEEK_SET);

	for (int i = 0; i < master.slavesQuantity; i++)
	{
		fread(&slave, SLAVE_SIZE, 1, slavesDb);
		fclose(slavesDb);

		if (slave.productId == productId)
		{
			return 0;
		}

		slavesDb = fopen(SLAVEDATAFILE, "r+b");
		fseek(slavesDb, slave.nextAddress, SEEK_SET);
	}

	fclose(slavesDb);

	return 1;
}

void allRegs()
{
	FILE* indexTable = fopen("master.ind", "rb");

	if (indexTable == NULL)
	{
		printf("Error: database files are not created yet\n");
		return;
	}

	int masterCount = 0;
	int slaveCount = 0;

	fseek(indexTable, 0, SEEK_END);
	int indAmount = ftell(indexTable) / sizeof(struct Index);

	struct Master master;

	char dummy[51];

	for (int i = 1; i <= indAmount; i++)
	{
		if (getMaster(&master, i, dummy))
		{
			masterCount++;
			slaveCount += master.slavesQuantity;

			printf("Master #%d has %d slave(s)\n", i, master.slavesQuantity);
		}
	}

	fclose(indexTable);

	printf("Total masters: %d\n", masterCount);
	printf("Total slaves: %d\n", slaveCount);
}
#pragma endregion
#pragma region SLAVE
void outputSlave(struct Slave slave, struct Master master)
{
	printf("Master: %s, %d scores\n", master.name, master.status);
	printf("Price: %d\n", slave.price);
	printf("Amount: %d\n", slave.amount);
}

void inputSlave(struct Slave* slave)
{
	int x;

	printf("Enter price: ");
	scanf("%d", &x);

	slave->price = x;

	printf("Enter amount: ");
	scanf("%d", &x);

	slave->amount = x;
}

void refresh(FILE* database)
{
	fclose(database);
	database = fopen(SLAVEDATAFILE, "r+b");
}

void pairAdress(FILE* database, struct Master master, struct Slave slave)
{
	refresh(database);								// Змінюємо режим на "читання з та запис у будь-яке місце"

	struct Slave previous;

	fseek(database, master.firstSlaveAddr, SEEK_SET);

	for (int i = 0; i < master.slavesQuantity; i++)		    // Пробігаємомо зв'язаний список до останньої поставки
	{
		fread(&previous, SLAVE_SIZE, 1, database);
		fseek(database, previous.nextAddress, SEEK_SET);
	}

	previous.nextAddress = slave.selfAddress;				// Зв'язуємо адреси
	fwrite(&previous, SLAVE_SIZE, 1, database);				// Заносимо оновлений запис назад до файлу
}

void unpairAdress(FILE* database, struct Slave previous, struct Slave slave, struct Master* master)
{
	if (slave.selfAddress == master->firstSlaveAddr)		// Немає попередника (перший запис)...
	{
		if (slave.selfAddress == slave.nextAddress)			// ...ще й немає наступника (запис лише один)
		{
			master->firstSlaveAddr = -1;					// Неможлива адреса для безпеки
		}
		else                                                // ...а наступник є,
		{
			master->firstSlaveAddr = slave.nextAddress;  // робимо його першим
		}
	}
	else                                                    // Попередник є...
	{
		if (slave.selfAddress == slave.nextAddress)			// ...але немає наступника (останній запис)
		{
			previous.nextAddress = previous.selfAddress;    // Робимо попередник останнім
		}
		else                                                // ... а разом з ним і наступник
		{
			previous.nextAddress = slave.nextAddress;		// Робимо наступник наступником попередника
		}

		fseek(database, previous.selfAddress, SEEK_SET);	// Записуємо оновлений попередник
		fwrite(&previous, SLAVE_SIZE, 1, database);			// назад до таблички
	}
}

void noteDeletedSlave(long address)
{
	FILE* garbageZone = fopen(SLAVEDATAFILE, "rb");			// "rb": відкриваємо бінарний файл для читання

	int garbageCount;
	fscanf(garbageZone, "%d", &garbageCount);

	long* delAddresses = malloc(garbageCount * sizeof(long)); // Виділяємо місце під список "сміттєвих" адрес

	for (int i = 0; i < garbageCount; i++)
	{
		fscanf(garbageZone, "%ld", delAddresses + i);		// Заповнюємо його
	}

	fclose(garbageZone);									// За допомогою цих двох команд
	garbageZone = fopen(SLAVEDATAFILE, "wb");				// повністю очищуємо файл зі "сміттям"
	fprintf(garbageZone, "%ld", garbageCount + 1);			// Записуємо нову кількість "сміттєвих" адрес

	for (int i = 0; i < garbageCount; i++)
	{
		fprintf(garbageZone, " %ld", delAddresses[i]);		// Заносимо "сміттєві" адреси назад...
	}

	fprintf(garbageZone, " %d", address);					// ...і дописуємо до них адресу щойно видаленого запису
	free(delAddresses);										// Звільняємо виділену під масив пам'ять
	fclose(garbageZone);									// Закриваємо файл
}

void rewriteGarbage(int garbageCount, FILE* garbageZone, struct Slave* record)
{
	long* delIds = malloc(garbageCount * sizeof(long));		// Виділяємо місце під список "сміттєвих" адрес

	for (int i = 0; i < garbageCount; i++)
	{
		fscanf(garbageZone, "%d", delIds + i);				// Заповнюємо його
	}

	record->selfAddress = delIds[0];						// Для запису замість логічно видаленої "сміттєвої"
	record->nextAddress = delIds[0];

	fclose(garbageZone);									// За допомогою цих двох команд
	fopen(SLAVEDATAFILE, "wb");							    // повністю очищуємо файл зі "сміттям"
	fprintf(garbageZone, "%d", garbageCount - 1);			// Записуємо нову кількість "сміттєвих" адрес

	for (int i = 1; i < garbageCount; i++)
	{
		fprintf(garbageZone, " %d", delIds[i]);				// Записуємо решту "сміттєвих" адрес
	}

	free(delIds);											// Звільняємо виділену під масив пам'ять
	fclose(garbageZone);									// Закриваємо файл
}

int insertSlave(struct Master master, struct Slave slave, char* error)
{
	slave.exists = 1;

	FILE* database = fopen(SLAVEDATAFILE, "a+b");
	FILE* garbageZone = fopen(SLAVEDATAFILE, "rb");

	int garbageCount;

	fscanf(garbageZone, "%d", &garbageCount);

	if (garbageCount)											// Наявні видалені записи
	{
		rewriteGarbage(garbageCount, garbageZone, &slave);
		refresh(database);								// Змінюємо режим доступу файлу
		fseek(database, slave.selfAddress, SEEK_SET);			// Ставимо курсор на "сміття" для подальшого перезапису
	}
	else                                                        // Видалених немає, пишемо в кінець файлу
	{
		fseek(database, 0, SEEK_END);

		int dbSize = ftell(database);

		slave.selfAddress = dbSize;
		slave.nextAddress = dbSize;
	}

	fwrite(&slave, SLAVE_SIZE, 1, database);					// Записуємо поставку до свого файлу

	if (!master.slavesQuantity)								    // Поставок ще немає, пишемо адресу першої
	{
		master.firstSlaveAddr = slave.selfAddress;
	}
	else                                                        // Поставки вже є, оновлюємо "адресу наступника" останньої
	{
		pairAdress(database, master, slave);
	}

	fclose(database);											// Закриваємо файл

	master.slavesQuantity++;										// Стало на одну поставку більше
	updateMaster(master, error);								// Оновлюємо запис постачальника

	return 1;
}

int getSlave(struct Master master, struct Slave* slave, int productId, char* error)
{
	if (!master.slavesQuantity)									// У постачальника немає поставок
	{
		strcpy(error, "This master has no slaves yet");
		return 0;
	}

	FILE* database = fopen(SLAVEDATAFILE, "rb");


	fseek(database, master.firstSlaveAddr, SEEK_SET);		// Отримуємо перший запис
	fread(slave, SLAVE_SIZE, 1, database);

	for (int i = 0; i < master.slavesQuantity; i++)				// Шукаємо потрібний запис по коду деталі
	{
		if (slave->productId == productId)						// Знайшли
		{
			fclose(database);
			return 1;
		}

		fseek(database, slave->nextAddress, SEEK_SET);
		fread(slave, SLAVE_SIZE, 1, database);
	}

	strcpy(error, "No such slave in database");					// Не знайшли
	fclose(database);
	return 0;
}

// На вхід подається поставка з оновленими значеннями, яку треба записати у файл
int updateSlave(struct Slave slave, int productId)
{
	FILE* database = fopen(SLAVEDATAFILE, "r+b");

	fseek(database, slave.selfAddress, SEEK_SET);
	fwrite(&slave, SLAVE_SIZE, 1, database);
	fclose(database);

	return 1;
}

int deleteSlave(struct Master master, struct Slave slave, int productId, char* error)
{
	FILE* database = fopen(SLAVEDATAFILE, "r+b");
	struct Slave previous;

	fseek(database, master.firstSlaveAddr, SEEK_SET);

	do		                                                    // Шукаємо попередника запису (його може й не бути,
	{															// тоді в попередника занесеться сам запис)
		fread(&previous, SLAVE_SIZE, 1, database);
		fseek(database, previous.nextAddress, SEEK_SET);
	} while (previous.nextAddress != slave.selfAddress && slave.selfAddress != master.firstSlaveAddr);

	unpairAdress(database, previous, slave, &master);
	noteDeletedSlave(slave.selfAddress);						// Заносимо адресу видаленого запису у "смітник"

	slave.exists = 0;											// Логічно не існуватиме

	fseek(database, slave.selfAddress, SEEK_SET);				// ...але фізично
	fwrite(&slave, SLAVE_SIZE, 1, database);					// записуємо назад
	fclose(database);

	master.slavesQuantity--;										// Однією поставкою менше
	updateMaster(master, error);

}
#pragma endregion
#pragma region  MASTER
void outputMaster(struct Master master)
{
	printf("Master\'s name: %s\n", master.name);
	printf("Master\'s status: %d\n", master.status);
}

void inputMaster(struct Master* master) // 
{
	char name[16];
	int status = 0;
	name[0] = '\0';

	printf("Enter master\'s name: ");
	scanf("%s", name);

	strcpy(master->name, name);

	printf("Enter master\'s status: ");
	scanf("%d", status);

	master->status = status;
}

void noteDeletedMaster(int id)
{
	FILE* garbageZone = fopen(MASTERGARBAGEFILE, "rb");		// "rb": відкриваємо бінарний файл для читання

	int garbageCount;
	fscanf(garbageZone, "%d", &garbageCount);

	int* delIds = malloc(garbageCount * sizeof(int));		// Виділяємо місце під список "сміттєвих" індексів

	for (int i = 0; i < garbageCount; i++)
	{
		fscanf(garbageZone, "%d", delIds + i);				// Заповнюємо його
	}

	fclose(garbageZone);									// За допомогою цих двох команд
	garbageZone = fopen(MASTERGARBAGEFILE, "wb");				// повністю очищуємо файл зі "сміттям"
	fprintf(garbageZone, "%d", garbageCount + 1);			// Записуємо нову кількість "сміттєвих" індексів

	for (int i = 0; i < garbageCount; i++)
	{
		fprintf(garbageZone, " %d", delIds[i]);				// Заносимо "сміттєві" індекси назад...
	}

	fprintf(garbageZone, " %d", id);						// ...і дописуємо до них індекс щойно видаленого запису
	free(delIds);											// Звільняємо виділену під масив пам'ять
	fclose(garbageZone);									// Закриваємо файл
}

void rewritGarbageById(int garbageCount, FILE* garbageZone, struct Master* record)
{
	int* delIds = malloc(garbageCount * sizeof(int));		// Виділяємо місце під список "сміттєвих" індексів

	for (int i = 0; i < garbageCount; i++)
	{
		fscanf(garbageZone, "%d", delIds + i);				// Заповнюємо його
	}

	record->id = delIds[0];									// Для запису замість логічно видаленого "сміттєвого"

	fclose(garbageZone);									// За допомогою цих двох команд
	fopen(MASTERGARBAGEFILE, "wb");							// повністю очищуємо файл зі "сміттям"
	fprintf(garbageZone, "%d", garbageCount - 1);			// Записуємо нову кількість "сміттєвих" індексів

	for (int i = 1; i < garbageCount; i++)
	{
		fprintf(garbageZone, " %d", delIds[i]);				// Записуємо решту "сміттєвих" індексів
	}

	free(delIds);											// Звільняємо виділену під масив пам'ять
	fclose(garbageZone);									// Закриваємо файл
}

int insertMaster(struct Master record)
{
	FILE* indexTable = fopen(MASTERINDEXFILE, "a+b");			// "a+b": відкрити бінарний файл
	FILE* masterData = fopen(MASTERDATAFILE, "a+b");				// для запису в кінець та читання
	FILE* garbageZone = fopen(MASTERGARBAGEFILE, "rb");		// "rb": відкрити бінарний файл для читання
	struct Index indexer;
	int garbageCount;

	fscanf(garbageZone, "%d", &garbageCount);

	if (garbageCount)										// Наявні "сміттєві" записи, перепишемо перший з них
	{
		rewritGarbageById(garbageCount, garbageZone, &record);

		fclose(indexTable);									// Закриваємо файли для зміни
		fclose(masterData);									// режиму доступу в подальшому

		indexTable = fopen(MASTERINDEXFILE, "r+b");				// Знову відкриваємо і змінюємо режим на
		masterData = fopen(MASTERDATAFILE, "r+b");				// "читання з та запис у довільне місце файлу"

		fseek(indexTable, (record.id - 1) * INDEXSIZE, SEEK_SET);
		fread(&indexer, INDEXSIZE, 1, indexTable);
		fseek(masterData, indexer.address, SEEK_SET);			// Ставимо курсор на "сміття" для подальшого перезапису	
	}
	else                                                    // Видалених записів немає
	{
		long indexerSize = INDEXSIZE;

		fseek(indexTable, 0, SEEK_END);						// Ставимо курсор у кінець файлу таблички

		if (ftell(indexTable))								// Розмір індексної таблички ненульовий (позиція від початку)
		{
			fseek(indexTable, -indexerSize, SEEK_END);		// Ставимо курсор на останній індексатор
			fread(&indexer, INDEXSIZE, 1, indexTable);	// Читаємо останній індексатор

			record.id = indexer.id + 1;						// Нумеруємо запис наступним індексом
		}
		else                                                // Індексна табличка порожня
		{
			record.id = 1;									// Індексуємо наш запис як перший
		}
	}

	record.firstSlaveAddr = -1;
	record.slavesQuantity = 0;

	fwrite(&record, MASTERSIZE, 1, masterData);				// Записуємо в потрібне місце БД-таблички передану структуру

	indexer.id = record.id;									// Вносимо номер запису в індексатор
	indexer.address = (record.id - 1) * MASTERSIZE;		// Вносимо адресу запису в індексатор
	indexer.status = 1;										// Прапорець існування запису

	printf("Your master\'s id is %d\n", record.id);

	fseek(indexTable, (record.id - 1) * INDEXSIZE, SEEK_SET);
	fwrite(&indexer, INDEXSIZE, 1, indexTable);			// Записуємо індексатор у відповідну табличку, куди треба
	fclose(indexTable);										// Закриваємо файли
	fclose(masterData);

	return 1;
}

int getMaster(struct Master* master, int id, char* error)
{
	FILE* indexTable = fopen(MASTERINDEXFILE, "rb");				// "rb": відкрити бінарний файл
	FILE* database = fopen(MASTERDATAFILE, "rb");				// тільки для читання

	if (!doesFileExists(indexTable, database, error))
	{
		return 0;
	}

	struct Index indexer;

	if (!doesIndexExists(indexTable, error, id))
	{
		return 0;
	}

	fseek(indexTable, (id - 1) * INDEXSIZE, SEEK_SET);	// Отримуємо індексатор шуканого запису
	fread(&indexer, INDEXSIZE, 1, indexTable);			// за вказаним номером

	if (!doesRegExists(indexer, error))
	{
		return 0;
	}

	fseek(database, indexer.address, SEEK_SET);				// Отримуємо шуканий запис з БД-таблички
	fread(master, sizeof(struct Master), 1, database);		// за знайденою адресою
	fclose(indexTable);										// Закриваємо файли
	fclose(database);

	return 1;
}

int updateMaster(struct Master master, char* error)
{
	FILE* indexTable = fopen(MASTERINDEXFILE, "r+b");			// "r+b": відкрити бінарний файл
	FILE* database = fopen(MASTERDATAFILE, "r+b");				// для читання та запису

	if (!doesFileExists(indexTable, database, error))
	{
		return 0;
	}

	struct Index indexer;
	int id = master.id;

	if (!doesIndexExists(indexTable, error, id))
	{
		return 0;
	}

	fseek(indexTable, (id - 1) * INDEXSIZE, SEEK_SET);	// Отримуємо індексатор шуканого запису
	fread(&indexer, INDEXSIZE, 1, indexTable);			// за вказаним номером

	if (!doesRegExists(indexer, error))
	{
		return 0;
	}

	fseek(database, indexer.address, SEEK_SET);				// Позиціонуємося за адресою запису в БД
	fwrite(&master, MASTERSIZE, 1, database);				// Оновлюємо запис
	fclose(indexTable);										// Закриваємо файли
	fclose(database);

	return 1;
}

int deleteMaster(int id, char* error)
{
	FILE* indexTable = fopen(MASTERINDEXFILE, "r+b");			// "r+b": відкрити бінарний файл
															// для читання та запису	
	if (indexTable == NULL)
	{
		strcpy(error, "database files are not created yet");
		return 0;
	}

	if (!doesIndexExists(indexTable, error, id))
	{
		return 0;
	}

	struct Master master;
	getMaster(&master, id, error);

	struct Index indexer;

	fseek(indexTable, (id - 1) * INDEXSIZE, SEEK_SET);	// Отримуємо індексатор шуканого запису
	fread(&indexer, INDEXSIZE, 1, indexTable);			// за вказаним номером

	indexer.status = 0;										// Запис логічно не існуватиме...

	fseek(indexTable, (id - 1) * INDEXSIZE, SEEK_SET);

	fwrite(&indexer, INDEXSIZE, 1, indexTable);			// ...але фізично буде присутній
	fclose(indexTable);										// Закриваємо файл [NB: якщо не закрити, значення не оновиться]

	noteDeletedMaster(id);									// Заносимо індекс видаленого запису до "сміттєвої зони"


	if (master.slavesQuantity)								// Були поставки, видаляємо всі
	{
		FILE* slavesDb = fopen(SLAVEDATAFILE, "r+b");
		struct Slave slave;

		fseek(slavesDb, master.firstSlaveAddr, SEEK_SET);

		for (int i = 0; i < master.slavesQuantity; i++)
		{
			fread(&slave, SLAVE_SIZE, 1, slavesDb);
			fclose(slavesDb);
			deleteSlave(master, slave, slave.productId, error);

			slavesDb = fopen(SLAVEDATAFILE, "r+b");
			fseek(slavesDb, slave.nextAddress, SEEK_SET);
		}

		fclose(slavesDb);
	}
	return 1;
}
#pragma endregion


int main()
{
	struct Master master;
	struct Slave slave;

	while (1)
	{
		int choice;
		int id;
		char error[51];

		printf("Choose option:\n0 - Quit\n1 - Insert Master\n2 - Get Master\n3 - Update Master\n4 - Delete Master\n5 - Insert Slave\n6 - Get Slave\n7 - Update Slave\n8 - Delete Slave\n9 - Info\n");
		scanf("%d", &choice);

		switch (choice)
		{
		case 0:
			return 0;

		case 1:
			inputMaster(&master);
			insertMaster(master);
			break;

		case 2:
			printf("Enter ID: ");
			scanf("%d", &id);
			getMaster(&master, id, error) ? outputMaster(master) : printf("Error: %s\n", error);
			break;

		case 3:
			printf("Enter ID: ");
			scanf("%d", &id);

			master.id = id;

			inputMaster(&master);
			updateMaster(master, error) ? printf("Updated successfully\n") : printf("Error: %s\n", error);
			break;

		case 4:
			printf("Enter ID: ");
			scanf("%d", &id);
			deleteMaster(id, error) ? printf("Deleted successfully\n") : printf("Error: %s\n", error);
			break;

		case 5:
			printf("Enter master\'s ID: ");
			scanf("%d", &id);

			if (getMaster(&master, id, error))
			{
				slave.masterId = id;
				printf("Enter product ID: ");
				scanf("%d", &id);

				if (doesPairUnique(master, id))		// Унікальність по коду деталі
				{
					slave.productId = id;
					inputSlave(&slave);
					insertSlave(master, slave, error);
					printf("Inserted successfully. To access, use master\'s and product\'s IDs\n");
				}
				else
				{
					printf("Error: non-unique product key\n");
				}
			}
			else
			{
				printf("Error: %s\n", error);
			}
			break;

		case 6:
			printf("Enter master\'s ID: ");
			scanf("%d", &id);

			if (getMaster(&master, id, error))
			{
				printf("Enter product ID: ");
				scanf("%d", &id);
				getSlave(master, &slave, id, error) ? outputSlave(slave, master) : printf("Error: %s\n", error);
			}
			else
			{
				printf("Error: %s\n", error);
			}
			break;

		case 7:
			printf("Enter master\'s ID: ");
			scanf("%d", &id);

			if (getMaster(&master, id, error))
			{
				printf("Enter product ID: ");
				scanf("%d", &id);

				if (getSlave(master, &slave, id, error))
				{
					inputSlave(&slave);
					updateSlave(slave, id, error);
					printf("Updated successfully\n");
				}
				else
				{
					printf("Error: %s\n", error);
				}
			}
			else
			{
				printf("Error: %s\n", error);
			}
			break;

		case 8:
			printf("Enter master\'s ID: ");
			scanf("%d", &id);

			if (getMaster(&master, id, error))
			{
				printf("Enter product ID: ");
				scanf("%d", &id);

				if (getSlave(master, &slave, id, error))
				{
					deleteSlave(master, slave, id, error);
					printf("Deleted successfully\n");
				}
				else
				{
					printf("Error: %s\n", error);
				}
			}
			else
			{
				printf("Error: %s\n", error);
			}
			break;

		case 9:
			allRegs();
			break;

		default:
			printf("Invalid input, please try again\n");
		}

		printf("---------\n");
	}

	return 0;
}