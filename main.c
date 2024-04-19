#include <stdio.h>


#define FRAME_SIZE 256 // размер фрейма 2^8 байт
#define COUNT_FRAMES 256 // всего 256 фреймов
#define TLB_SIZE 16 // 16 записей в TLB
#define PAGE_SIZE 256 // размер страницы 2^8 байт

FILE *address_txt;
FILE *backing_store_bin;
FILE *correct2_txt;

struct page{
    int offset;
    int page_number;
    int logical_address;
};

struct page_frame_number {
    int page_number;
    int frame_number;
};

int physical_memory [COUNT_FRAMES][FRAME_SIZE];
struct page_frame_number TLB[TLB_SIZE];
struct page_frame_number PAGE_TABLE[COUNT_FRAMES];

const int offset_mask = (1<<8)-1; // 255

signed char buf[256];
signed char value;

int next_available_index = 0;
int cached = 0; // количесвто закэшированных
int hit = 0;
int page_miss = 0;

int read_store(int page_number) {
    // доступно ли место в таблице страниц
    if (next_available_index >= COUNT_FRAMES) {
        return -1;
    }

    // считывание данных из backing store в буфер
    fseek(backing_store_bin, page_number * PAGE_SIZE, SEEK_SET); // установка позиции в потоке данных, смещение отсчитывается от начала файла со смещением в 256 байт * номер страницы
    fread(buf, sizeof(signed char), PAGE_SIZE, backing_store_bin); // считываем данные из потока по байту по странице 256 раз ( ее размер )

    // копирования всех байт из буфера buf в соответствующий фрейм физической памяти.
    for (int i = 0; i < PAGE_SIZE; i++) {
        physical_memory[next_available_index][i] = buf[i];
    }

    PAGE_TABLE[next_available_index].page_number = page_number;
    PAGE_TABLE[next_available_index].frame_number = next_available_index;

    int new_frame = next_available_index;
    next_available_index++;

    return new_frame;
}

void insert_TLB_FIFO(int page_number, int frame_number) {
    int i;
    // сдвигом освобождаем место для новой записи если находим в TLB
    for (i = 0; i < cached; i++) {
        if (TLB[i].page_number == page_number) {
            while (i < cached - 1) {
                TLB[i] = TLB[i + 1];
                i++;
            }
            break;
        }
    }
    // Если мы не находим совпадение по номеру страницы
    if (i == cached)
        for (int j =0; j < i; j++)
            TLB[j] = TLB[j + 1];

    TLB[i].page_number = page_number;
    TLB[i].frame_number = frame_number;

    if (cached < TLB_SIZE -1 )
        cached++;

}

struct page get_page(int logical_address) {
    struct page current_page;
    current_page.offset = logical_address & offset_mask;
    current_page.page_number = (logical_address >> 8) & offset_mask;
    current_page.logical_address = logical_address;
    return current_page;
}

void process_virtual_page(struct page current_page)
{
    int page_number = current_page.page_number;
    int offset = current_page.offset;
    int logical_address = current_page.logical_address;

    int frame_number = -1;

    // TLB
    for (int i = 0; i < cached + 1; i++) {
        if (TLB[i].page_number == page_number) {
            frame_number = TLB[i].frame_number;
            hit++;
            break;
        }
    }

    // page table
    if (frame_number == -1) {
        for (int i = 0; i < next_available_index;i++) {
            if (PAGE_TABLE[i].page_number == page_number) {
                frame_number = PAGE_TABLE[i].frame_number;
                break;
            }
        }
    }

    // backing_store_bin
    if (frame_number == -1) {
        frame_number = read_store(page_number);
        page_miss++;
    }

    insert_TLB_FIFO(page_number,frame_number);
    value = physical_memory[frame_number][offset];

    fprintf(correct2_txt, "Virtual address: %d Physical address: %d Value: %d\n", logical_address, (frame_number << 8) | offset, value);

}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        return -1;
    }

    address_txt = fopen(argv[1], "r");
    backing_store_bin = fopen("BACKING_STORE.bin", "rb"); // чтение BACKING_STORE в доичном формате
    correct2_txt = fopen(argv[2], "w"); // Открываем файл для записи

    if (address_txt == NULL || backing_store_bin == NULL || correct2_txt == NULL) {
        return -1;
    }

    int processed_address = 0; // количество обработанных адресов
    int logical_address; // логический адрес ( виртуальный адрес )
    while (fscanf(address_txt, "%d", &logical_address) == 1) {

        process_virtual_page(get_page(logical_address));
        processed_address++;
    }

    double Page_error_rate = page_miss / (double)processed_address;
    double TLB_error_rate = hit / (double)processed_address;

    fprintf(correct2_txt, "Частота ошибок страниц = %.3f\n",Page_error_rate);
    fprintf(correct2_txt, "Частота попаданий в TLB  = %.3f\n", TLB_error_rate);

    fclose(address_txt);
    fclose(backing_store_bin);
    fclose(correct2_txt);
}
