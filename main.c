#include <stdio.h>

#define FRAME_SIZE 256
#define FRAMES 256
#define TLB_SIZE 16
#define CHUNK 256

FILE *address_file;
FILE *backing_store;
FILE *output_file;

struct page_frame {
    int page_number;
    int frame_number;
};

int physical_mem [FRAMES][FRAME_SIZE];
struct page_frame TLB[TLB_SIZE];
struct page_frame PAGE_TABLE[FRAMES];
int hit = 0;
int page_miss = 0;
signed char buffer[CHUNK];
int first_available_frame = 0;
int first_available_page_table_index = 0;
signed char value;
int TLB_caches = 0; // кешированные записи

int read_from_store(int pageNum) {
    // проверка, что есть место в таблице страниц
    if (first_available_frame >= FRAMES || first_available_page_table_index >= FRAMES) {
        fprintf(stderr, "Memory error\n");
        return -1;
    }

    // считывание данных из backing store в буфер
    fseek(backing_store, pageNum * CHUNK, SEEK_SET);
    fread(buffer, sizeof(signed char), CHUNK, backing_store);

    // копирование данных из буфера в физическую память
    for (int i = 0; i < CHUNK; i++) {
        physical_mem[first_available_frame][i] = buffer[i];
    }

    // обновление таблицы страниц
    PAGE_TABLE[first_available_page_table_index].page_number = pageNum;
    PAGE_TABLE[first_available_page_table_index].frame_number = first_available_frame;

    // обновление индексво для следующей записи
    int new_frame = first_available_frame;
    first_available_frame++;
    first_available_page_table_index++;

    return new_frame;
}

void insert_TLB(int page_num, int frame_num) { // сдвигаем записи вверх удаляя самую старую, для добавления новой
    int i;
    for (i = 0; i < TLB_caches; i++) {
        if (TLB[i].page_number == page_num) {
            while (i < TLB_caches - 1) { // свдигаем записи вверх для освобождения места под новую запись
                TLB[i] = TLB[i + 1];
                i++;
            }
            break;
        }
    }

    if (i == TLB_caches) // если не совпално вставляем в конец
        for (int j =0; j < i; j++)
            TLB[j] = TLB[j + 1];

    TLB[i].page_number = page_num;
    TLB[i].frame_number = frame_num;

    if (TLB_caches < TLB_SIZE -1 )
        TLB_caches++;

}

void get_page(int logical_add) {
    // смещение
    int pageNum = (logical_add & 0xFFFF) >>8;
    int offset = logical_add & 0xFF;

    int frameNum = -1;

    for (int i = 0; i < TLB_caches + 1; i++) { // пробуем получить фрейм из tlb
        if (TLB[i].page_number == pageNum) {
            frameNum = TLB[i].frame_number;
            hit++;
            break;
        }
    }

    if (frameNum == -1) {
        for (int i = 0; i < first_available_page_table_index;i++) { // пробуем получить фрейм из таблицы страниц
            if (PAGE_TABLE[i].page_number == pageNum) {
                frameNum = PAGE_TABLE[i].frame_number;
                break;
            }
        }
    }

    if (frameNum == -1) { // получаем фрейм из backing_store
        frameNum = read_from_store(pageNum);
        page_miss++;
    }

    insert_TLB(pageNum,frameNum); // вставляем в tlb
    value = physical_mem[frameNum][offset]; // поулчаем значение по физ адресу

    fprintf(output_file, "Virtual address: %d Physical address: %d Value: %d\n", logical_add, (frameNum << 8) | offset, value); // Заменяем printf на fprintf для записи в файл
}

int main(int argc, char *argv[]) {
    if (argc != 3) { // Проверяем наличие аргументов для ввода и вывода
        fprintf(stderr, "input/output error");
        return -1;
    }

    address_file = fopen(argv[1], "r");
    backing_store = fopen("BACKING_STORE.bin", "rb");
    output_file = fopen(argv[2], "w"); // Открываем файл для записи

    if (address_file == NULL) {
        fprintf(stderr, "address.txt error");
        return -1;
    }

    if (backing_store == NULL) {
        fprintf(stderr, "BACKING_STORE.bin error");
        return -1;
    }

    if (output_file == NULL) { // Проверяем, удалось ли открыть файл для записи
        fprintf(stderr, "output file error");
        return -1;
    }

    int translated_add = 0;
    int logical_add;
    while (fscanf(address_file, "%d", &logical_add) == 1) {
        get_page(logical_add);
        translated_add++;
    }

    fprintf(output_file, "Обработанные адреса = %d\n", translated_add); // Записываем статистику в файл
    double miss_stat = page_miss / (double)translated_add;
    double TLB_stat = hit / (double)translated_add;

    fprintf(output_file, "Ошибки страницы = %d\n", page_miss);
    fprintf(output_file, "Процент ошибок страницы = %.3f\n",miss_stat);
    fprintf(output_file, "TLB попадания = %d\n", hit);
    fprintf(output_file, "процент TLB попаданий = %.3f\n", TLB_stat);

    fclose(address_file);
    fclose(backing_store);
    fclose(output_file); // Закрываем файл после использования
}
