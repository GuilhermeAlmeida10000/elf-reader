/* Author(s): <Guilherme Bachenheimer de Almeida>
 * Creates operating system image suitable for placement on a boot disk
*/
/* TODO: Comment on the status of your submission. Largely unimplemented */
#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_FILE "./image"
#define ARGS "[--extended] <bootblock> <executable-file> ..."

#define SECTOR_SIZE 512       /* floppy sector size in bytes */
#define BOOTLOADER_SIG_OFFSET 0x1fe /* offset for boot loader signature */
// more defines...

/* Reads in an executable file in ELF format*/
Elf32_Phdr * read_exec_file(FILE **execfile, char *filename, Elf32_Ehdr **ehdr) {
    *execfile = fopen(filename, "rb");
    if (*execfile == NULL) {
        perror("Erro ao abrir o arquivo executável");
        return NULL;
    }

    // Lendo o cabeçalho ELF
    *ehdr = malloc(sizeof(Elf32_Ehdr));
    fread(*ehdr, sizeof(Elf32_Ehdr), 1, *execfile);

    // Verificando se é um arquivo ELF válido
    if ((*ehdr)->e_ident[EI_MAG0] != ELFMAG0 || (*ehdr)->e_ident[EI_MAG1] != ELFMAG1 ||
        (*ehdr)->e_ident[EI_MAG2] != ELFMAG2 || (*ehdr)->e_ident[EI_MAG3] != ELFMAG3) {
        fprintf(stderr, "Arquivo não é um ELF válido\n");
        fclose(*execfile);
        free(*ehdr);
        return NULL;
    }

    // Lendo o cabeçalho do programa ELF
    Elf32_Phdr *phdr = malloc(sizeof(Elf32_Phdr) * (*ehdr)->e_phnum);
    fseek(*execfile, (*ehdr)->e_phoff, SEEK_SET);
    fread(phdr, sizeof(Elf32_Phdr), (*ehdr)->e_phnum, *execfile);

    return phdr;
}


/* Writes the bootblock to the image file */
void write_bootblock(FILE **imagefile, FILE *bootfile, Elf32_Ehdr *boot_header, Elf32_Phdr *boot_phdr) {
    // Obtendo o tamanho do bootblock em bytes
    int bootblock_size = boot_phdr->p_filesz;

    // Alocando memória para armazenar o bootblock
    char *bootblock_data = malloc(bootblock_size);

    // Lendo os dados do bootblock do arquivo
    fseek(bootfile, boot_phdr->p_offset, SEEK_SET);
    fread(bootblock_data, bootblock_size, 1, bootfile);

    // Escrevendo os dados do bootblock no arquivo de imagem
    fwrite(bootblock_data, bootblock_size, 1, *imagefile);

    // Liberando a memória alocada
    free(bootblock_data);
}


/* Writes the kernel to the image file */
void write_kernel(FILE **imagefile,FILE *kernelfile,Elf32_Ehdr *kernel_header, Elf32_Phdr *kernel_phdr)
{
    // Obtendo o tamanho do kernel em bytes
    int kernel_size = kernel_phdr->p_filesz;

    // Alocando memória para armazenar o kernel
    char *kernel_data = malloc(kernel_size);

    // Lendo os dados do kernel do arquivo
    fseek(kernelfile, kernel_phdr->p_offset, SEEK_SET);
    fread(kernel_data, kernel_size, 1, kernelfile);

    // Escrevendo os dados do kernel no arquivo de imagem
    fwrite(kernel_data, kernel_size, 1, *imagefile);

    // Liberando a memória alocada
    free(kernel_data);
}

/* Counts the number of sectors in the kernel */
int count_kernel_sectors(Elf32_Ehdr *kernel_header, Elf32_Phdr *kernel_phdr)
{   // Calculando o tamanho do kernel em bytes
    int kernel_size_bytes = kernel_phdr->p_filesz;

    // Calculando o número de setores ocupados pelo kernel
    int num_sectors = (kernel_size_bytes + SECTOR_SIZE - 1) / SECTOR_SIZE;

    return num_sectors;
}

/* Records the number of sectors in the kernel */
void record_kernel_sectors(FILE **imagefile,Elf32_Ehdr *kernel_header, Elf32_Phdr *kernel_phdr, int num_sec)
{
    // Posicionando o ponteiro de arquivo no local onde o número de setores do kernel deve ser gravado
    fseek(*imagefile, BOOTLOADER_SIG_OFFSET, SEEK_SET);

    // Escrevendo o número de setores no arquivo de imagem
    fwrite(&num_sec, sizeof(int), 1, *imagefile);    
}


/* Prints segment information for --extended option */
void extended_opt(Elf32_Phdr *bph, int k_phnum, Elf32_Phdr *kph, int num_sec)
{
    printf("Número de setores usados pela imagem: %d\n", num_sec);

    printf("Informações do segmento do bootblock:\n");
    printf("Tipo: %u\n", bph->p_type);
    printf("Offset: %u\n", bph->p_offset);
    printf("Tamanho do arquivo: %u\n", bph->p_filesz);
    printf("Tamanho da memória: %u\n", bph->p_memsz);
    printf("Flags: %u\n", bph->p_flags);
    printf("Alinhamento: %u\n", bph->p_align);

    printf("Informações do segmento do kernel:\n");
    for (int i = 0; i < k_phnum; i++) {
        printf("Segmento %d:\n", i + 1);
        printf("Tipo: %u\n", kph[i].p_type);
        printf("Offset: %u\n", kph[i].p_offset);
        printf("Tamanho do arquivo: %u\n", kph[i].p_filesz);
        printf("Tamanho da memória: %u\n", kph[i].p_memsz);
        printf("Flags: %u\n", kph[i].p_flags);
        printf("Alinhamento: %u\n", kph[i].p_align);
    }

    printf("Tamanho do kernel em setores: %d\n", num_sec);
}
// more helper functions...

/* MAIN */
int main(int argc, char **argv)
{
	FILE *kernelfile, *bootfile,*imagefile;  // file pointers for bootblock,kernel and image
	Elf32_Ehdr *boot_header   = malloc(sizeof(Elf32_Ehdr)); // bootblock ELF header
	Elf32_Ehdr *kernel_header = malloc(sizeof(Elf32_Ehdr)); // kernel ELF header
	
	Elf32_Phdr *boot_program_header;   // bootblock ELF program header
	Elf32_Phdr *kernel_program_header; // kernel ELF program header

	if (argc < 3) {
        fprintf(stderr, "Uso: %s <bootblock> <kernel>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *bootblock_filename = argv[1];
    char *kernel_filename = argv[2];

    FILE *imagefile = fopen(IMAGE_FILE, "wb");
    if (imagefile == NULL) {
        perror("Erro ao abrir o arquivo de imagem");
        return EXIT_FAILURE;
    }

    Elf32_Phdr *boot_program_header = read_exec_file(&bootfile, bootblock_filename, boot_header);
    if (boot_program_header == NULL) {
        return EXIT_FAILURE;
    }

    write_bootblock(&imagefile, bootfile, boot_header, boot_program_header);

    Elf32_Phdr *kernel_program_header = read_exec_file(&kernelfile, kernel_filename, kernel_header);
    if (kernel_program_header == NULL) {
        return EXIT_FAILURE;
    }

    write_kernel(&imagefile, kernelfile, kernel_header, kernel_program_header);

    int num_sectors = count_kernel_sectors(kernel_header, kernel_program_header);

    record_kernel_sectors(&imagefile, kernel_header, kernel_program_header, num_sectors);

    if (argc > 3 && !strncmp(argv[3], "--extended", 11)) {
        extended_opt(boot_program_header, kernel_header->e_phnum, kernel_program_header, num_sectors);
    }

    fclose(imagefile);
    fclose(bootfile);
    fclose(kernelfile);
    free(boot_header);
    free(kernel_header);
    free(boot_program_header);
    free(kernel_program_header);

    return EXIT_SUCCESS;
} // ends main()



