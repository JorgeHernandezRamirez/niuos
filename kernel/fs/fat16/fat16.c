#include "fat16.h"
#include "../../drivers/disk.h"
#include "../../mm/pmm.h"
#include "../../drivers/screen.h"
#include "../../lib/lib.h"

#define NAME_DELETED 0xE5
#define NAME_NULL 0x00

#define ATTR_READ_ONLY  0x01
#define ATTR_HIDDEN     0x02
#define ATTR_SYSTEM     0x04
#define ATTR_VOLUME_ID  0x08
#define ATTR_DIRECTORY  0x10
#define ATTR_ARCHIVE    0x20
#define ATTR_LONG_NAME  0x0F

typedef struct __attribute__((packed)) {
    uint8_t  name[8];
    uint8_t  ext[3];
    uint8_t  attr;
    uint8_t  nt_reserved;
    uint8_t  create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high; // 0 en FAT12/16
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} fat_dir_entry_t;

//Cacheamos en memoria el boot_sector, fat16_1 (primera copia) y root_dir
fat16_boot_sector_t *boot_sector;
uint16_t *fat16_1;
fat_dir_entry_t *root_dir;

/* LBA absoluto donde arranca la particion FAT16 en el disco (fs.dmg), fijado por fat16_init() */
static uint32_t _fs_lba_start = 36;

/* no hay <string.h> en este toolchain freestanding */
static void _memcpy(void *dst, const void *src, uint32_t n){
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < n; i++){
        d[i] = s[i];
    }
}

void _read_fat16_boot_sector(){
    boot_sector = (fat16_boot_sector_t *)vmalloc(sizeof(fat16_boot_sector_t));
    disk_driver_instance.read_sectors(_fs_lba_start, 1, (uint16_t *)boot_sector);
}

fat16_boot_sector_t *fat16_get_boot_sector(){
    return boot_sector;
}

void _print_boot_sector(){
    if(!FAT16_DEBUG){
        return;
    }
    fat16_boot_sector_t *bs = boot_sector;

    screen_print("oem_name: ");
    screen_print_buf(bs->oem_name, 8);
    screen_print("\nbytes_per_sector: ");
    screen_print_int(bs->bytes_per_sector);
    screen_print("\nsectors_per_cluster: ");
    screen_print_int(bs->sectors_per_cluster);
    screen_print("\nreserved_sectors: ");
    screen_print_int(bs->reserved_sectors);
    screen_print("\nnum_fats: ");
    screen_print_int(bs->num_fats);
    screen_print("\nroot_entries: ");
    screen_print_int(bs->root_entries);
    screen_print("\ntotal_sectors_16: ");
    screen_print_int(bs->total_sectors_16);
    screen_print("\nmedia_descriptor: ");
    screen_print_hex32(bs->media_descriptor);
    screen_print("\nsectors_per_fat: ");
    screen_print_int(bs->sectors_per_fat);
    screen_print("\nsectors_per_track: ");
    screen_print_int(bs->sectors_per_track);
    screen_print("\nnum_heads: ");
    screen_print_int(bs->num_heads);
    screen_print("\nhidden_sectors: ");
    screen_print_int(bs->hidden_sectors);
    screen_print("\ntotal_sectors_32: ");
    screen_print_int(bs->total_sectors_32);
    screen_print("\ndrive_number: ");
    screen_print_hex32(bs->drive_number);
    screen_print("\nboot_signature: ");
    screen_print_hex32(bs->boot_signature);
    screen_print("\nvolume_id: ");
    screen_print_hex32(bs->volume_id);
    screen_print("\nvolume_label: ");
    screen_print_buf(bs->volume_label, 11);
    screen_print("\nfs_type: ");
    screen_print_buf(bs->fs_type, 8);
    screen_print("\nboot_sector_sig: ");
    //screen_print_hex32(bs->boot_sector_sig);
    //screen_print("\n");
}

uint16_t _size_fat16(){
    return boot_sector->sectors_per_fat * boot_sector->bytes_per_sector;
}

void _print_fat16_1(){
    if(!FAT16_DEBUG){
        return;
    }
    for(uint16_t i = 0; i < _size_fat16() / 2; i++){
        screen_print_int(i);
        screen_print("-");
        screen_print_int(fat16_1[i]);
        screen_print("  ");
    }
}

/**
 * Read first copy fat16_1
 */
void _read_fat16_1(){
    fat16_1 = (uint16_t *)vmalloc(_size_fat16());
    disk_driver_instance.read_sectors(
        _fs_lba_start + boot_sector->reserved_sectors,
        boot_sector->sectors_per_fat,
        (uint16_t *)fat16_1);
}

uint16_t _size_fat16_root_dir(){
    return boot_sector->root_entries * 32;
}

uint16_t _root_dir_num_sectors(){
    //Root entries es el numero de entradas de 32 bytes que ocupa el root entry
    return _size_fat16_root_dir() / boot_sector->bytes_per_sector;
}

void _read_fat16_root_dir(){
    root_dir = (fat_dir_entry_t *)vmalloc(_size_fat16_root_dir());
    disk_driver_instance.read_sectors(
        _fs_lba_start + boot_sector->reserved_sectors + boot_sector->sectors_per_fat * boot_sector->num_fats,
        _root_dir_num_sectors(),
        (uint16_t *)root_dir);
}

void _print_fat16_root_dir(){
    if(!FAT16_DEBUG){
        return;
    }
    for(uint16_t i = 0; i < boot_sector->root_entries; i++){
        if (root_dir[i].name[0] == 0x00) break; // fin de entradas validas
        if ((uint8_t)root_dir[i].name[0] == 0xE5) continue; // entrada borrada

        screen_print_buf(root_dir[i].name, 8);
        screen_print(".");
        screen_print_buf(root_dir[i].ext, 3);
        screen_print("  size=");
        screen_print_int(root_dir[i].file_size);
        screen_print("  cluster=");
        screen_print_int(root_dir[i].first_cluster_low);
        screen_print("\n");
    }
}

void fat16_init(uint32_t lba_start){
    _fs_lba_start = lba_start;
    init_disk_driver();
    disk_driver_instance.open();
    _read_fat16_boot_sector();
    _print_boot_sector();
    _read_fat16_1();
    _print_fat16_1();
    _read_fat16_root_dir(); //son 16MB. puede ser que sea demasiada. Primera version cacheamos esto en memoria
    _print_fat16_root_dir();
}

uint8_t _count_delimiter(char *path){
    int i = 0, counter = 0;
    while(path[i] != '\0'){
        if(path[i] == FAT16_DELIMITER){
            counter++;
        }
        i++;
    }
    return counter;
}

char **_init_slice_out(char *path, uint8_t* size){
    *size = _count_delimiter(path);
    char **out = (char **)vmalloc(sizeof(char *) * *size);
    for(uint8_t i = 0; i < *size; i++){
        out[i] = (char *)vmalloc(sizeof(char) * FAT16_MAX_FILENAME_SIZE);
    }
    return out;
}

char **_slice(char *path, uint8_t* size){
    char **out = _init_slice_out(path, size);
    int out_i = 0, out_j=0, i = 0;
    if(path[i] == FAT16_DELIMITER){
        i++;
    }
    while(path[i] != '\0'){
        if(path[i] == FAT16_DELIMITER){
            out[out_i][out_j] = '\0';
            out_j = 0;
            out_i++;
            i++;
        }
        else{
            out[out_i][out_j++] = path[i++];
        }

    }
    return out;
}

uint8_t _to_upper(uint8_t c){
    if (c >= 'a' && c <= 'z'){
        return c - 'a' + 'A';
    }
    return c;
}

uint8_t _to_lower(uint8_t c){
    if (c >= 'A' && c <= 'Z'){
        return c - 'A' + 'a';
    }
    return c;
}

void _format_83(char *parent_name, uint8_t *out){
    for (int i = 0; i < FAT16_MAX_FILENAME_SIZE; i++){
        out[i] = ' ';
    }

    int dot = -1;
    int len = 0;
    while (parent_name[len] != '\0'){
        if (parent_name[len] == '.' && dot == -1){
            dot = len;
        }
        len++;
    }

    int name_len = (dot == -1) ? len : dot;
    if (name_len > 8) name_len = 8;
    for (int i = 0; i < name_len; i++){
        out[i] = _to_upper((uint8_t)parent_name[i]);
    }

    if (dot != -1){
        int ext_len = len - dot - 1;
        if (ext_len > 3) ext_len = 3;
        for (int i = 0; i < ext_len; i++){
            out[8 + i] = _to_upper((uint8_t)parent_name[dot + 1 + i]);
        }
    }
}

fat_dir_entry_t *_cluster_root_dir(char *parent_name, uint16_t *out_offset){
    uint8_t formatted_name[FAT16_MAX_FILENAME_SIZE];
    _format_83(parent_name, formatted_name);

    for(uint16_t i = 0; i < boot_sector->root_entries; i++){
        if (root_dir[i].name[0] == 0x00) break; // fin de entradas validas
        if ((uint8_t)root_dir[i].name[0] == 0xE5) continue; // entrada borrada

        // el struct es packed, asi que name[8]+ext[3] son 11 bytes contiguos en memoria
        int match = 1;
        for (int j = 0; j < FAT16_MAX_FILENAME_SIZE; j++){
            if (root_dir[i].name[j] != formatted_name[j]){
                match = 0;
                break;
            }
        }
        if (match){
            if (out_offset) *out_offset = (uint16_t)(i * sizeof(fat_dir_entry_t));
            return &root_dir[i];
        }
    }
    return 0; // no encontrado
}

uint32_t _size_cluster(){
    return boot_sector->bytes_per_sector * boot_sector->sectors_per_cluster; //4KB
}

/* LBA absoluto donde arranca la region fija del directorio raiz (FAT16 la
   reserva aparte, justo despues de las copias de la FAT -- no es un
   cluster normal, no se direcciona con numero de cluster). */
uint32_t _root_dir_lba(){
    return _fs_lba_start
        + boot_sector->reserved_sectors
        + boot_sector->sectors_per_fat * boot_sector->num_fats;
}

/* LBA absoluto donde arrancan los datos de un cluster normal (>=2), ya en
   la zona de datos (despues de la region del directorio raiz). */
uint32_t _cluster_lba(uint16_t cluster){
    return _root_dir_lba() + _root_dir_num_sectors()
        + (cluster - 2) * boot_sector->sectors_per_cluster;
}

fat_dir_entry_t *_read_fat16_data(uint16_t cluster){
    fat_dir_entry_t *data = (fat_dir_entry_t *)vmalloc(_size_cluster());
    disk_driver_instance.read_sectors(
        _cluster_lba(cluster),
        boot_sector->sectors_per_cluster,
        (uint16_t *)data);
    return data;
}

/* cache del ultimo cluster leido para fat16_readdir(): antes releia el
   cluster entero (8 sectores por ATA PIO) en CADA llamada, aunque siguiera
   escaneando el mismo cluster -- para un directorio de 130 ficheros eso son
   cientos de comandos PIO identicos seguidos, y en la practica el driver
   ATA (sin timeouts ni reintentos) termina devolviendo basura (0xFFFF,
   bus flotante) despues de unos ~130 comandos consecutivos. Cachear evita
   el problema de raiz ademas de ser lo correcto: solo hay que releer de
   disco cuando el cluster realmente cambia. */
static uint16_t _readdir_cache_cluster = 0xFFFF;
static fat_dir_entry_t *_readdir_cache_data = NULL;

fat_dir_entry_t *_read_fat16_data_cached(uint16_t cluster){
    if (_readdir_cache_data != NULL && _readdir_cache_cluster == cluster){
        return _readdir_cache_data;
    }
    _readdir_cache_data = _read_fat16_data(cluster);
    _readdir_cache_cluster = cluster;
    return _readdir_cache_data;
}

fat_dir_entry_t *_cluster_data(uint16_t cluster_number, char *filename, uint16_t *out_offset){
    fat_dir_entry_t *cluster = _read_fat16_data_cached(cluster_number);

    uint8_t formatted_name[FAT16_MAX_FILENAME_SIZE];
    _format_83(filename, formatted_name);

    uint16_t num_entries = _size_cluster() / sizeof(fat_dir_entry_t);
    for (uint16_t i = 0; i < num_entries; i++){
        if (cluster[i].name[0] == 0x00) break; // fin de entradas validas
        if ((uint8_t)cluster[i].name[0] == 0xE5) continue; // entrada borrada

        int match = 1;
        for (int j = 0; j < FAT16_MAX_FILENAME_SIZE; j++){
            if (cluster[i].name[j] != formatted_name[j]){
                match = 0;
                break;
            }
        }
        if (match){
            if (out_offset) *out_offset = (uint16_t)(i * sizeof(fat_dir_entry_t));
            return &cluster[i];
        }
    }

    return NULL; //NOT FOUND
}

uint32_t *_fat16_data_raw(uint16_t cluster){
    return (uint32_t *)_read_fat16_data(cluster);
}

void _map_entry_to_ff(FileFAT* ff, fat_dir_entry_t *aux, uint16_t container_cluster, uint16_t entry_offset, uint8_t container_is_root){
    ff->cluster = aux->first_cluster_low;
    ff->reader.cluster = aux->first_cluster_low;
    ff->file_size = aux->file_size;
    ff->type = aux->attr;
    ff->dirent_parent.cluster = container_cluster;
    ff->dirent_parent.cluster_offset = entry_offset;
    ff->dirent_parent.root = container_is_root;
}


uint8_t _do_init_ff_no_root_file(char **filename_splitted, uint8_t size, FileFAT* ff){
    uint16_t container_cluster = 0;
    uint8_t container_is_root = 1;
    uint16_t entry_offset = 0;
    fat_dir_entry_t *aux = _cluster_root_dir(filename_splitted[0], &entry_offset);
    if(!aux){
        return 0;
    }
    for(int i = 1; i < size; i++){
        container_cluster = aux->first_cluster_low; // el directorio recien encontrado pasa a ser el contenedor
        container_is_root = 0;
        aux = _cluster_data(container_cluster, filename_splitted[i], &entry_offset);
        if(aux == NULL){
            return 0;
        }
    }
    if(aux != NULL){
        _map_entry_to_ff(ff, aux, container_cluster, entry_offset, container_is_root);
        return 1;
    }
    return 0;
}

uint8_t _do_init_ff_no_root(char *filename, FileFAT* ff){
    uint8_t size;
    char **filename_splitted = _slice(filename, &size);
    return _do_init_ff_no_root_file(filename_splitted, size, ff);
}

uint8_t _do_init_ff(char *filename, FileFAT* ff){
    if(is_root(filename)){
        ff->root = 1;
        ff->type = ATTR_DIRECTORY;
        return 1;
    }
    return _do_init_ff_no_root(filename, ff);
}

uint8_t _init_ff(char *filename, FileFAT *ff, int flags){
    ff->cluster = 0;
    ff->file_size = 0;
    ff->reader.cluster = 0;
    ff->reader.cluster_offset = 0;
    ff->root = 0;
    ff->dirent_parent.root = 0;
    ff->flags = flags;
    return _do_init_ff(filename, ff);
}

char *_file_name_from_file_path(char *file_path){
    if(is_root(file_path)){
        return file_path;
    }
    char *aux = char_last_pos(file_path);
    if(*aux == FAT16_DELIMITER){
        aux--;
    }
    while(aux != file_path && *(aux - 1) != FAT16_DELIMITER){
        aux--;
    }
    int size = strlen(aux);
    char *value = (char *)vmalloc(size + 1);
    memcpy(value, aux, size);
    value[size] = '\0';
    return value;
}

char *_get_parent_from_file_path(char *file_path){
     if(is_root(file_path)){
        return "/";
    }
    char *aux = char_last_pos(file_path);
    if(*aux == FAT16_DELIMITER){
        aux--;
    }
    while(*aux != FAT16_DELIMITER){
        aux--;
    }
    int size = strlen(file_path) - strlen(aux);
    if(size == 0){
        size = 1;
    }
    char *value = (char *)vmalloc(size);
    memcpy(value, file_path, size + 1);
    value[size] = '\0';
    return value;
}

int16_t _alloc_fat(){
    for(int i = 0; i < _size_fat16() / 2; i++){
        if(fat16_1[i] == 0x0000){
            fat16_1[i] = LAST_CLUSTER;
            return i;
        }
    }
    screen_print("Full filesystem. Can't reserve cluster\n");
    return -1;
}

void _flush_fat(){
    uint32_t fat1_lba = _fs_lba_start + boot_sector->reserved_sectors;
    for (uint8_t i = 0; i < boot_sector->num_fats; i++){
        disk_driver_instance.write_sectors(
            fat1_lba + i * boot_sector->sectors_per_fat,
            boot_sector->sectors_per_fat,
            (uint16_t *)fat16_1);
    }
}

FileFAT * _get_parent_file(char *file_path){
    char *name = _get_parent_from_file_path(file_path);
    return fat16_open(name, O_RDONLY);
}

uint8_t _is_end_cluster(uint16_t cluster){
    return cluster==LAST_CLUSTER;
}

uint8_t _is_last_cluster_from_cluster(uint16_t cluster){
    return _is_end_cluster(fat16_1[cluster]);
}

uint8_t _is_last_cluster(FileFAT* ff){
    return _is_last_cluster_from_cluster(ff->reader.cluster);
}

uint16_t _last_cluster_num_bytes(FileFAT *ff){
    //Return bytes to read on lasat cluster
    return ff->file_size % _size_cluster();
}

uint8_t _end_of_file(FileFAT* ff){
    return ff->reader.cluster_offset >= _last_cluster_num_bytes(ff) && _is_last_cluster(ff);
}

uint8_t _enough_bytes_current_cluster(FileFAT* ff, uint32_t bytes_left){
    return _size_cluster() - ff->reader.cluster_offset >= bytes_left;
}


// 1. entry->name[0] == 0x00 (Fin directorio)
// 2. entry->name[0] == 0xE5 (Entrada borrada)
uint8_t _null_entry(fat_dir_entry_t *entry){
    return entry->name[0] == NAME_NULL;
}

uint8_t _deleted_entry(fat_dir_entry_t *entry){
    return entry->name[0] == NAME_DELETED;
}

uint8_t _archive_entry(fat_dir_entry_t *entry){
    return entry->attr & ATTR_ARCHIVE;
}

uint8_t _volume_id_entry(fat_dir_entry_t *entry){
    return entry->attr & ATTR_VOLUME_ID;
}

uint8_t _dir_entry(fat_dir_entry_t *entry){
    return entry->attr & ATTR_DIRECTORY;
}

fat_dir_entry_t *_dirent_container_buffer(uint8_t parent_is_root, uint16_t parent_cluster){
    if (parent_is_root){
        return root_dir;
    }
    return _read_fat16_data_cached(parent_cluster);
}

void _flush_dirent_container_root(){
     disk_driver_instance.write_sectors(
            _root_dir_lba(), _root_dir_num_sectors(), (uint16_t *)root_dir);
}

void _flush_dirent_container(uint16_t parent_cluster, fat_dir_entry_t *entry){
    disk_driver_instance.write_sectors(
            _cluster_lba(parent_cluster), boot_sector->sectors_per_cluster, (uint16_t *)entry);
}

fat_dir_entry_t *_alloc_entry_root(FileFAT *parent, uint16_t *cluster_parent, fat_dir_entry_t **entry_parent){
    *entry_parent = root_dir;
    *cluster_parent = 0;
    for(uint16_t i = 0; i < boot_sector->root_entries; i++){
        if(_null_entry(&root_dir[i]) || _deleted_entry(&root_dir[i])){
            return &root_dir[i];
        }
    }
    screen_print("Root directory full\n");
    return NULL;
}

fat_dir_entry_t *_alloc_entry_no_root(FileFAT *parent, uint16_t *cluster_parent, fat_dir_entry_t **entry_parent){
    uint16_t current_cluster = parent->cluster;
    fat_dir_entry_t *aux = _read_fat16_data_cached(parent->cluster);
    *entry_parent = aux;
    int offset = 0;
    uint8_t found = 0;
    while(!found){
        while(offset < _size_cluster() && !_null_entry(aux) && !_deleted_entry(aux)){
            aux++;
            offset += sizeof(fat_dir_entry_t);
        }
        if(_null_entry(aux) || _deleted_entry(aux)){
            *cluster_parent = current_cluster;
            return aux;
        }
        //Si no es end of cluster se intenta en el siguiente
        if(!_is_last_cluster_from_cluster(current_cluster)){
            current_cluster = fat16_1[current_cluster];
        }
        else{
            //Si es end of cluster se allocate el siguiente
            int16_t new_cluster = _alloc_fat();
            if(new_cluster == -1){
                screen_print("Can't allocate cluster for new file\n");
                return NULL;
            } 
            fat16_1[current_cluster] = new_cluster;
            current_cluster = new_cluster;
        }
        aux = _read_fat16_data_cached(current_cluster);
        *entry_parent = aux;
        offset = 0;
    }
    return NULL;
}

fat_dir_entry_t *_alloc_entry(FileFAT *parent, uint16_t *cluster_parent, fat_dir_entry_t **entry_parent){
    if(parent->root){
        return _alloc_entry_root(parent, cluster_parent, entry_parent);
    }
    return _alloc_entry_no_root(parent, cluster_parent, entry_parent);
}

FileFAT * _do_fat16_create(FileFAT *parent, char *filename){
    int16_t cluster_content_file = _alloc_fat();
    if(cluster_content_file == -1){
        screen_print("Can't allocate cluster for new file\n");  
        for(;;){}
        return NULL;
    }
    uint16_t cluster_parent; //Cluster del parent en el que se creo
    fat_dir_entry_t *entry_parent; //Puntero a la posición inicial del cluster
    fat_dir_entry_t *entry = _alloc_entry(parent, &cluster_parent, &entry_parent);
    if(entry == NULL){
        return NULL;
    }
    entry->first_cluster_low = cluster_content_file;
    entry->first_cluster_high = 0;
    entry->attr = ATTR_ARCHIVE;
    entry->file_size = 0;
    uint8_t formatted_name[FAT16_MAX_FILENAME_SIZE];
    _format_83(filename, formatted_name);
    _memcpy(entry->name, formatted_name, 8);
    _memcpy(entry->ext, formatted_name + 8, 3);
    FileFAT* file = (FileFAT *)vmalloc(sizeof(FileFAT));
    _map_entry_to_ff(file, entry, cluster_parent, (uint8_t *)entry-(uint8_t *)entry_parent, parent->root);
    _flush_fat();
    if(parent->root){
        _flush_dirent_container_root();
    }
    else{
        _flush_dirent_container(cluster_parent, entry_parent);
    }
    return file;
}

FileFAT* _fat16_create(char *file_path, int flags){
    if(is_root(file_path)){
        //Can't create root dir
        return NULL;
    }
    FileFAT *parent = _get_parent_file(file_path);
    if(!parent){
        screen_print("Parent directory does not exists\n");
        return NULL;
    }
    return _do_fat16_create(parent, _file_name_from_file_path(file_path));
}

int64_t _flag_create(int flags){
    return flags & O_CREAT;
}

int _flag_truncate(int flags){
    return flags & O_TRUNC;
}

void _flush_disk(FileFAT *ff, void (*function)(fat_dir_entry_t *entry, FileFAT *ff)){
    fat_dir_entry_t *container = _dirent_container_buffer(ff->dirent_parent.root, ff->dirent_parent.cluster);
    fat_dir_entry_t *entry = (fat_dir_entry_t *)((uint8_t *)container + ff->dirent_parent.cluster_offset);
    function(entry, ff);
    if(ff->dirent_parent.root){
        _flush_dirent_container_root();
    }
    else{
        _flush_dirent_container(ff->dirent_parent.cluster, container);
    }
}

void _update_size_on_entry(fat_dir_entry_t *entry, FileFAT *ff){
    entry->file_size = ff->file_size;
}

void _fat_free_chain(uint16_t start_cluster){
    uint16_t cluster = start_cluster;
    while (!_is_end_cluster(cluster)){
        uint16_t next = fat16_1[cluster];
        fat16_1[cluster] = 0;
        cluster = next;
    }
}

void _flush_fat_truncate(FileFAT *ff, uint16_t num_cluster, uint16_t num_cluster_to_let){
    int i = 0;
    uint16_t cluster = ff->cluster;
    while(i < num_cluster_to_let){
        cluster = fat16_1[cluster];
        i++;
    }
    uint16_t last_cluster = cluster;
    while(i < num_cluster && !_is_end_cluster(cluster)){
        uint16_t aux = fat16_1[cluster];
        fat16_1[cluster] = 0;
        cluster = aux;
        i++;
    }
    fat16_1[last_cluster] = LAST_CLUSTER;
    _flush_fat();
}

int _fat16_truncate(FileFAT *ff, uint16_t size){
    uint16_t num_cluster = ff->file_size / _size_cluster();
    uint16_t num_cluster_to_let = size / _size_cluster();
    ff->file_size = size;
    _flush_disk(ff, _update_size_on_entry);
    if(num_cluster_to_let >= num_cluster){
        return 0;      
    }
    _flush_fat_truncate(ff, num_cluster, num_cluster_to_let);
    return 0;
}

int fat16_truncate(char *file_path, int length){
    FileFAT *ff = fat16_open(file_path, O_RDONLY);
    return _fat16_truncate(ff, length);
}

FileFAT *_do_fat16_open_not_found(char *file_path, int flags){
    if(_flag_create(flags)){
        return _fat16_create(file_path, flags);
    }
    return NULL;
}

FileFAT *fat16_open(char *file_path, int flags){
    FileFAT* file = (FileFAT *)vmalloc(sizeof(FileFAT));
    uint8_t found =_init_ff(file_path, file, flags);
    if(!found){
        return _do_fat16_open_not_found(file_path, flags);
    }
    if(_flag_truncate(flags)){
        _fat16_truncate(file, 0);
    }
    return file;
}

uint8_t _is_file(FileFAT *ff){
    return !(ff->type & ATTR_DIRECTORY) &&
           !(ff->type & ATTR_VOLUME_ID) &&
           ff->type != ATTR_LONG_NAME;
}

uint8_t _is_directory(FileFAT *ff){
    return ff->type & ATTR_DIRECTORY;
}

FileFAT* fat16_opendir(char *filename){
    filename = fs_format_file_path(filename);
    FileFAT *file = fat16_open(filename, O_RDONLY);
    if(is_null(file) || !_is_directory(file)){
        return NULL;
    }
    return file;
}

uint16_t _do_next_cluster(FileFAT* ff){
    if(_is_last_cluster(ff)){
        return ff->reader.cluster;
    }
    return fat16_1[ff->reader.cluster];
}

void _next_cluster(FileFAT* ff){
    if(ff->reader.cluster_offset >= _size_cluster()){
        ff->reader.cluster_offset = 0;
        ff->reader.cluster = _do_next_cluster(ff);
        ff->reader.offset += _size_cluster();
    }
}

uint16_t _get_num_bytes_left_last_cluster(FileFAT *ff, uint32_t bytes_left){
    uint16_t bytes_to_read_last_cluster = _last_cluster_num_bytes(ff);
    if(ff->reader.cluster_offset + bytes_left >= bytes_to_read_last_cluster){
        return bytes_to_read_last_cluster - ff->reader.cluster_offset;
    }
    return bytes_left;
}

uint16_t fat16_read(FileFAT* ff, uint32_t bytes, uint32_t *buf){
    uint32_t i = 0;
    uint16_t cluster_in_raw = 0xFFFF;
    uint32_t *raw = NULL;
    while(i < bytes && ff->reader.offset < ff->file_size){
        if(cluster_in_raw != ff->reader.cluster){
            raw = _fat16_data_raw(ff->reader.cluster);
            cluster_in_raw = ff->reader.cluster;
        }
        uint32_t bytes_to_read = bytes - i;
        uint32_t left_cluster = _size_cluster() - ff->reader.cluster_offset;
        uint32_t left_file = ff->file_size - ff->reader.offset;
        if(bytes_to_read > left_cluster){
            bytes_to_read = left_cluster;
        }
        if(bytes_to_read > left_file){
            bytes_to_read = left_file;
        }
        _memcpy((uint8_t *)buf + i,
                (uint8_t *)raw + ff->reader.cluster_offset,
                bytes_to_read);
        ff->reader.cluster_offset += bytes_to_read;
        ff->reader.offset += bytes_to_read;
        i += bytes_to_read;
        if(ff->reader.cluster_offset >= _size_cluster()){ //_next_cluster() no vale aqui: sube reader.offset por su cuenta
            ff->reader.cluster_offset = 0;
            ff->reader.cluster = _do_next_cluster(ff);
        }
    }
    return (uint16_t)i;
}

uint32_t _fat16_write_bytes(FileFAT *ff, uint8_t *src, uint32_t count){
    uint32_t written = 0;
    uint32_t *raw = _fat16_data_raw(ff->reader.cluster);
    while(written < count){
        uint32_t room = _size_cluster() - ff->reader.cluster_offset;
        uint32_t chunk = count - written;
        if(chunk > room){
            chunk = room;
        }

        uint8_t *dst = (uint8_t *)raw + ff->reader.cluster_offset;
        if(src != NULL){
            _memcpy(dst, src + written, chunk);
        }
        else{
            for(uint32_t z = 0; z < chunk; z++){
                dst[z] = 0;
            }
        }
        disk_driver_instance.write_sectors(
            _cluster_lba(ff->reader.cluster),
            boot_sector->sectors_per_cluster,
            (uint16_t *)raw);

        ff->reader.cluster_offset += chunk;
        written += chunk;

        if(written < count){
            uint16_t next_cluster;
            if(_is_last_cluster_from_cluster(ff->reader.cluster)){
                int16_t new_cluster = _alloc_fat();
                if(new_cluster == -1){
                    screen_print("Can't allocate cluster for write\n");
                    break;
                }
                fat16_1[ff->reader.cluster] = new_cluster;
                next_cluster = new_cluster;
            }
            else{
                next_cluster = fat16_1[ff->reader.cluster];
            }
            ff->reader.cluster = next_cluster;
            ff->reader.cluster_offset = 0;
            raw = _fat16_data_raw(ff->reader.cluster);
        }
    }
    return written;
}

void _flush_file_size(FileFAT *ff){
    fat_dir_entry_t *container = _dirent_container_buffer(ff->dirent_parent.root, ff->dirent_parent.cluster);
    fat_dir_entry_t *entry = (fat_dir_entry_t *)((uint8_t *)container + ff->dirent_parent.cluster_offset);
    entry->file_size = ff->file_size;
    if(ff->dirent_parent.root){
        _flush_dirent_container_root();
    }
    else{
        _flush_dirent_container(ff->dirent_parent.cluster, container);
    }
}

uint16_t fat16_write(FileFAT *ff, uint32_t bytes, uint32_t *buf){
    //Si el cursor quedo mas alla del tamaño real (p.ej. seek mas alla del
    //EOF), primero hay que rellenar ese hueco con ceros y reservar los
    //clusters que haga falta para llegar hasta ahi.
    if(ff->reader.offset > ff->file_size){
        uint32_t gap = ff->reader.offset - ff->file_size;
        _fat16_write_bytes(ff, NULL, gap);
        ff->file_size = ff->reader.offset;
    }

    uint32_t written = _fat16_write_bytes(ff, (uint8_t *)buf, bytes);
    ff->reader.offset += written;
    if(ff->reader.offset > ff->file_size){
        ff->file_size = ff->reader.offset;
    }

    _flush_fat();
    _flush_file_size(ff);

    return (uint16_t)written;
}

uint8_t fat16_read_(FileFAT* ff, uint32_t bytes, uint32_t *buf){
    _memcpy(buf, _read_fat16_data(ff->cluster), bytes); //Sustituir memcopy -> crearlo
    return 1;
}

int _set_name_to_dirent(fat_dir_entry_t *entry, dirent *value){
    int len = 0;
    for (int i = 0; i < 8 && entry->name[i] != ' '; i++){
        value->d_name[len++] = (char)_to_lower(entry->name[i]);
    }
    if (entry->ext[0] != ' '){
        value->d_name[len++] = '.';
        for (int i = 0; i < 3 && entry->ext[i] != ' '; i++){
            value->d_name[len++] = (char)_to_lower(entry->ext[i]);
        }
    }
    value->d_name[len] = '\0';
    return len;
}

dirent *_build_direct_object(fat_dir_entry_t *entry, FileFAT *ff){
    dirent *value = (dirent *)vmalloc(sizeof(dirent));
    value->d_ino = ff->reader.cluster_offset;
    value->d_off = ff->reader.cluster_offset;
    int len = _set_name_to_dirent(entry, value);
    value->d_reclen = (unsigned short)len;
    value->d_type = (entry->attr & 0x10) ? DT_DIR : DT_REG;
    return value;
}

dirent *_do_fat16_readdir_no_root(FileFAT *ff){
    uint8_t *entry = (uint8_t *)_read_fat16_data_cached(ff->reader.cluster) + ff->reader.cluster_offset;
    while(ff->reader.cluster_offset < _size_cluster() && !_null_entry((fat_dir_entry_t *)entry) &&
          _deleted_entry((fat_dir_entry_t *)entry)){
        entry = entry + sizeof(fat_dir_entry_t);
        ff->reader.cluster_offset += sizeof(fat_dir_entry_t); 
    }
    //Caso fin de cluster
    if(ff->reader.cluster_offset >= _size_cluster()){
        if(_is_last_cluster(ff)){
            return NULL;
        }
        else{
           _next_cluster(ff);
           return fat16_readdir(ff);
        }
    }
    //Caso fin de fat_dir_entry_t en el cluster. No mas entradas
    if(_null_entry((fat_dir_entry_t *)entry)){
        return NULL;
    }

    //Caso se encontro algo
    ff->reader.cluster_offset += sizeof(fat_dir_entry_t); 
    return _build_direct_object((fat_dir_entry_t *)entry, ff);
}

uint8_t _is_valid_entry(fat_dir_entry_t *entry){
    return !_volume_id_entry(entry) && (_archive_entry(entry) || _dir_entry(entry));
}

uint8_t _is_entry_to_consider_root(fat_dir_entry_t *entry){
    return !_null_entry((fat_dir_entry_t *)entry) &&
          !_deleted_entry((fat_dir_entry_t *)entry) && _is_valid_entry(entry);
}

dirent *_do_fat16_readdir_root(FileFAT *ff){
    uint8_t *entry = (uint8_t *)root_dir + ff->reader.cluster_offset; //cluster_offset is used on the root dir to point the offset of the entry
    while(ff->reader.cluster_offset < _size_fat16_root_dir() && !_is_entry_to_consider_root((fat_dir_entry_t *)entry)){
        entry = entry + sizeof(fat_dir_entry_t);
        ff->reader.cluster_offset += sizeof(fat_dir_entry_t); 
    }
    //Caso fin de cluster
    if(ff->reader.cluster_offset >= _size_fat16_root_dir()){
        return NULL;
    }
    //Caso se encontro algo
    ff->reader.cluster_offset += sizeof(fat_dir_entry_t); 
    return _build_direct_object((fat_dir_entry_t *)entry, ff);
}

dirent *fat16_readdir(FileFAT *ff){
    return ff->root ? _do_fat16_readdir_root(ff) : _do_fat16_readdir_no_root(ff);
}

uint16_t _min(uint16_t a, uint16_t b){
    return a < b ? a : b;
}

void _do_fat16_seek_set(FileFAT *ff, long offset){
    if(offset < 0){
        offset = 0;
    }
    int num_cluster = offset / _size_cluster();
    int offset_last_cluster = offset % _size_cluster();
    int cluster = ff->cluster;
    int cluster_ant = cluster;
    int i = 0;
    while(!_is_end_cluster(cluster) && i < num_cluster){
        cluster_ant = cluster;
        cluster = fat16_1[cluster];
        i++;
    }
    //Si final de cluster
    if(_is_end_cluster(cluster)){
        ff->reader.cluster = cluster_ant;
        ff->reader.cluster_offset = _last_cluster_num_bytes(ff);
        ff->reader.offset = offset;
        return;
    }
    //Si no
    ff->reader.cluster = cluster;
    ff->reader.cluster_offset = offset_last_cluster;
    ff->reader.offset = offset;
}

void _do_fat16_seek_cur(FileFAT *ff, long offset){
    int offset_to_go = ff->reader.offset + offset;
    _do_fat16_seek_set(ff, offset_to_go);
}

void _do_fat16_seek_end(FileFAT *ff, long offset){
    _do_fat16_seek_set(ff, ff->file_size + offset);
}

int fat16_seek(FileFAT *ff, long offset, int whence){
    switch (whence){
        case SEEK_CUR:
            _do_fat16_seek_cur(ff, offset);
            break;
        case SEEK_END:
            _do_fat16_seek_end(ff, offset);
            break;
        case SEEK_SET:
            _do_fat16_seek_set(ff, offset);
            break;

        default:
            return -1;
    }
    return 0;
}

void _do_mark_deleted(fat_dir_entry_t *entry, FileFAT *ff){
    entry->name[0] = NAME_DELETED;
}

void _mark_dirent_deleted(FileFAT *ff){
    _flush_disk(ff, _do_mark_deleted);
}

void _do_fat16_remove(FileFAT *fat){
    _mark_dirent_deleted(fat);
    _fat_free_chain(fat->cluster);
    _flush_fat();
}

/**
 * Only remove if is file
 */
int fat16_remove(char *filename){
    FileFAT *ff = fat16_open(filename, O_RDONLY);
    if(is_null(ff) || !_is_file(ff)){
        return -1;
    }
    _do_fat16_remove(ff);
    return 0;
}

uint8_t fat16_isdir(FileFAT *ff){
    return _is_directory(ff);
}

int fat16_stat(FileFAT *ff, stat_t *st){
    if(is_null(ff) || is_null(st)){
        return -1;
    }
    uint32_t cluster_size = _size_cluster();
    st->st_dev = 0; //solo hay un disco
    st->st_ino = ff->root ? 0 : ff->cluster; //FAT16 no tiene inodos: usamos el cluster inicial
    st->st_mode = _is_directory(ff) ? S_IFDIR : S_IFREG;
    st->st_mode |= (ff->type & ATTR_READ_ONLY) ? 0555 : 0755; //FAT16 solo distingue read-only
    st->st_nlink = 1;
    st->st_size = ff->file_size;
    st->st_blksize = cluster_size;
    st->st_blocks = (ff->file_size + cluster_size - 1) / cluster_size;
    return 0;
}