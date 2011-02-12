#include "symbols.h"

int callchain = -1;
struct symb_arr all_syms;
struct symb_arr kern_symb_arr = { .name = "kernel" };
struct symb unknown_symbol = { .name = "[unknown symbol(s)]" };

struct symb* symb_new() {
   struct symb *k = malloc(sizeof(*k));
   k->name = NULL;
   memset(k->samples, 0, sizeof(k->samples));
   memset(k->core_samples, 0, sizeof(k->core_samples));
   k->parent = NULL;
   k->calling_apps_name = NULL;
   k->direct_callers = NULL;
   k->called_symbols = NULL;
   k->callers = NULL;
   return k;
}

struct symb_arr* symb_arr_new() {
   struct symb_arr *arr = malloc(sizeof(*arr));
   arr->entries = NULL;
   arr->size = arr->index = 0;
   arr->sorted_by_hex = 0;
   arr->name = NULL;
   return arr;
}

void symb_print(struct symb *k) {
   printf("%llx: %s\n", k->hex, k->name);
}

void symb_arr_print(struct symb_arr *arr) {
   int i;
   for (i = 0; i < arr->index; i++) {
      symb_print(arr->entries[i]);
   }
}

static int symb_sort_by_hex(const void *a, const void *b) {
   return (*(struct symb **) a)->hex > (*(struct symb **) b)->hex;
}

void symb_sort(int(*sort_fun)(const void *a, const void *b), struct symb_arr *arr) {
   if (sort_fun == symb_sort_by_hex)
      arr->sorted_by_hex = 1;
   else
      arr->sorted_by_hex = 0;
   qsort(arr->entries, arr->index, sizeof(struct symb*), sort_fun);
}

/**
 * Insert and find symbols
 */
static void symb_insert(struct symb *k, struct symb_arr *arr) {
   k->parent = arr;
   if (arr->index == arr->size) {
      if (arr->size == 0)
         arr->size = 500;
      arr->size *= 2;
      arr->entries = realloc(arr->entries, arr->size * sizeof(*arr->entries));
   }
   arr->entries[arr->index++] = k;
   arr->sorted_by_hex = 0; // We don't know if the symbol has been inserted in the right position

   if (all_syms.index == all_syms.size) {
      if (all_syms.size == 0)
         all_syms.size = 500;
      all_syms.size *= 2;
      all_syms.entries = realloc(all_syms.entries, all_syms.size
               * sizeof(*all_syms.entries));
   }
   all_syms.entries[all_syms.index++] = k;
   all_syms.sorted_by_hex = 0;
}

struct symb* symb_find(long long unsigned ip, struct symb_arr *arr) {
   if (!arr->sorted_by_hex) {
      symb_sort(symb_sort_by_hex, arr);
   }
   size_t bottom = 0, middle, top = arr->index - 1;
   while (1) {
      middle = (top + bottom) / 2;
      assert(middle >= 0 && middle < arr->index);
      if (ip < arr->entries[middle]->hex) {
         top = middle;
      }
      else {
         if (middle < arr->index - 1 && ip < arr->entries[middle + 1]->hex) {
            return arr->entries[middle];
         }
         if (bottom == middle) {
            return arr->entries[bottom];
         }
         bottom = middle;
      }
      if (top == bottom) {
         return arr->entries[top];
      }
   }
}

struct symb* symb_get(int index, struct symb_arr *arr) {
   return arr->entries[index];
}

/**
 * KERNEL
 */

struct symb * symb_get_kern(int index) {
   return kern_symb_arr.entries[index];
}

void symb_add_kern(char *file) {
   char *line = NULL;
   size_t n;
   FILE * sym_file = fopen(file, "r");
   assert(sym_file);
   char useless_char;
   char module_name[512];
   while (!feof(sym_file)) {
      int line_len = getline(&line, &n, sym_file);
      if (line_len < 0)
         break;
      line[--line_len] = '\0';

      struct symb *k = symb_new();
      k->name = malloc(400);
      int
               nb_lex =
                        sscanf(line, "%llx %c %s\t[%s", &k->hex, &useless_char, k->name, module_name);
      if (nb_lex < 3)
         continue;
      if (nb_lex == 4) { // Module
         strcat(k->name, " [");
         strcat(k->name, module_name);
      }
      symb_insert(k, &kern_symb_arr);
   }
}

/**
 * USERLAND: mostly taken from Ingo powerfulness : parse .so files to add symbols
 */
GHashTable* parsed_files;
struct symb_arr *symb_find_exe(char *file) {
   return g_hash_table_lookup(parsed_files, file);
}

static Elf_Scn *elf_section_by_name(Elf *elf, GElf_Ehdr *ep, GElf_Shdr *shp, const char *name, size_t *idx) {
   Elf_Scn *sec = NULL;
   size_t cnt = 1;
   while ((sec = elf_nextscn(elf, sec)) != NULL) {
      char *str;

      gelf_getshdr(sec, shp);
      str = elf_strptr(elf, ep->e_shstrndx, shp->sh_name);
      if (!strcmp(name, str)) {
         if (idx)
            *idx = cnt;
         break;
      }
      ++cnt;
   }
   return sec;
}
static inline uint8_t elf_sym__type(const GElf_Sym *sym) {
   return GELF_ST_TYPE(sym->st_info);
}
static inline int elf_sym__is_function(const GElf_Sym *sym) {
   return elf_sym__type(sym) == STT_FUNC && sym->st_name != 0 && sym->st_shndx
            != SHN_UNDEF;
}
static inline int elf_sym__is_label(const GElf_Sym *sym) {
   return elf_sym__type(sym) == STT_NOTYPE && sym->st_name != 0
            && sym->st_shndx != SHN_UNDEF && sym->st_shndx != SHN_ABS;
}
static inline const char *elf_sym__name(const GElf_Sym *sym, const Elf_Data *symstrs) {
   return symstrs->d_buf + sym->st_name;
}
static inline const char *elf_sec__name(const GElf_Shdr *shdr, const Elf_Data *secstrs) {
   return secstrs->d_buf + shdr->sh_name;
}
static inline int elf_sec__is_text(const GElf_Shdr *shdr, const Elf_Data *secstrs) {
   return strstr(elf_sec__name(shdr, secstrs), "text") != NULL;
}
void init_symb() {
   if (parsed_files)
      return;
   parsed_files = g_hash_table_new(g_str_hash, g_str_equal);
   if (elf_version(EV_CURRENT) == EV_NONE)
      die("Cannot initiate libelf");

   struct symb_arr *vdso = symb_arr_new();
   vdso->name = "[vdso]";
   struct symb *s = symb_new();
   s->hex = 0;
   s->name = strdup("[vdso]");
   symb_insert(s, vdso);
   g_hash_table_insert(parsed_files, vdso->name, vdso);
   g_hash_table_insert(parsed_files, kern_symb_arr.name, &kern_symb_arr);
}

#define elf_section__for_each_rel(reldata, pos, pos_mem, idx, nr_entries) \
        for (idx = 0, pos = gelf_getrel(reldata, 0, &pos_mem); \
             idx < nr_entries; \
             ++idx, pos = gelf_getrel(reldata, idx, &pos_mem))

#define elf_section__for_each_rela(reldata, pos, pos_mem, idx, nr_entries) \
        for (idx = 0, pos = gelf_getrela(reldata, 0, &pos_mem); \
             idx < nr_entries; \
             ++idx, pos = gelf_getrela(reldata, idx, &pos_mem))
static int symb_synthesize_plt_symbols(struct symb_arr *arr, Elf *elf) {
   uint32_t nr_rel_entries, idx;
   GElf_Sym sym;
   uint64_t plt_offset;
   GElf_Shdr shdr_plt;
   GElf_Shdr shdr_rel_plt, shdr_dynsym;
   Elf_Data *reldata, *syms, *symstrs;
   Elf_Scn *scn_plt_rel, *scn_symstrs, *scn_dynsym;
   size_t dynsym_idx;
   GElf_Ehdr ehdr;
   char sympltname[1024];
   int nr = 0, symidx, err = 0;

   if (gelf_getehdr(elf, &ehdr) == NULL)
      goto out_elf_end;

   scn_dynsym
            = elf_section_by_name(elf, &ehdr, &shdr_dynsym, ".dynsym", &dynsym_idx);
   if (scn_dynsym == NULL)
      goto out_elf_end;

   scn_plt_rel
            = elf_section_by_name(elf, &ehdr, &shdr_rel_plt, ".rela.plt", NULL);
   if (scn_plt_rel == NULL) {
      scn_plt_rel
               = elf_section_by_name(elf, &ehdr, &shdr_rel_plt, ".rel.plt", NULL);
      if (scn_plt_rel == NULL)
         goto out_elf_end;
   }

   err = -1;

   if (shdr_rel_plt.sh_link != dynsym_idx)
      goto out_elf_end;

   if (elf_section_by_name(elf, &ehdr, &shdr_plt, ".plt", NULL) == NULL)
      goto out_elf_end;

   /*
    * Fetch the relocation section to find the idxes to the GOT
    * and the symbols in the .dynsym they refer to.
    */
   reldata = elf_getdata(scn_plt_rel, NULL);
   if (reldata == NULL)
      goto out_elf_end;

   syms = elf_getdata(scn_dynsym, NULL);
   if (syms == NULL)
      goto out_elf_end;

   scn_symstrs = elf_getscn(elf, shdr_dynsym.sh_link);
   if (scn_symstrs == NULL)
      goto out_elf_end;

   symstrs = elf_getdata(scn_symstrs, NULL);
   if (symstrs == NULL)
      goto out_elf_end;

   nr_rel_entries = shdr_rel_plt.sh_size / shdr_rel_plt.sh_entsize;
   plt_offset = shdr_plt.sh_offset;

   if (shdr_rel_plt.sh_type == SHT_RELA) {
      GElf_Rela pos_mem, *pos;

      elf_section__for_each_rela(reldata, pos, pos_mem, idx,
               nr_rel_entries) {
         symidx = GELF_R_SYM(pos->r_info);
         plt_offset += shdr_plt.sh_entsize;
         gelf_getsym(syms, symidx, &sym);
         snprintf(sympltname, sizeof(sympltname), "%s@plt", elf_sym__name(&sym, symstrs));
         struct symb *s = symb_new();
         s->hex = plt_offset;
         s->name = strdup(sympltname);
         symb_insert(s, arr);
         ++nr;
      }
   }
   else if (shdr_rel_plt.sh_type == SHT_REL) {
      GElf_Rel pos_mem, *pos;
      elf_section__for_each_rel(reldata, pos, pos_mem, idx,
               nr_rel_entries) {
         symidx = GELF_R_SYM(pos->r_info);
         plt_offset += shdr_plt.sh_entsize;
         gelf_getsym(syms, symidx, &sym);
         snprintf(sympltname, sizeof(sympltname), "%s@plt", elf_sym__name(&sym, symstrs));
         struct symb *s = symb_new();
         s->hex = plt_offset;
         s->name = strdup(sympltname);
         symb_insert(s, arr);
         ++nr;
      }
   }

   err = 0;
   out_elf_end: if (err == 0)
      return nr;
   fprintf(stderr, "%s: problems reading PLT info.\n", __func__);
   return 0;
}

#define elf_symtab__for_each_symbol(syms, nr_syms, idx, sym) \
        for (idx = 0, gelf_getsym(syms, idx, &sym);\
             idx < nr_syms; \
             idx++, gelf_getsym(syms, idx, &sym))
#ifndef DMGL_PARAMS
#define DMGL_PARAMS      (1 << 0)       /* Include function args */
#define DMGL_ANSI        (1 << 1)       /* Include const, volatile, etc */
#endif
void symb_add_exe(char *file) {
   init_symb();
   if (symb_find_exe(file))
      return;

   struct symb_arr *arr = symb_arr_new();
   arr->name = strdup(file);

   int fd = open(file, O_RDONLY);
   if (fd < 0) {
      struct symb *s = symb_new();
      s->hex = 0;
      s->name = strdup(file);
      symb_insert(s, arr);
      goto out_close;
   }
   else {
      struct symb *dummy = symb_new();
      dummy->hex = 0;
      dummy->name = "[dummy]";
      symb_insert(dummy, arr);
   }

   Elf_Data *symstrs, *secstrs;
   uint32_t nr_syms;
   uint32_t idx;
   GElf_Ehdr ehdr;
   GElf_Shdr shdr;
   Elf_Data *syms;
   GElf_Sym sym;
   Elf_Scn *sec, *sec_strndx;
   int adjust_symbols = 0;
   int nr = 0, kernel = !strcmp("[kernel]", file);
   Elf *elf = elf_begin(fd, ELF_C_READ, NULL);
   if (elf == NULL)
      die("%s: cannot read %s ELF file.\n", __func__, file);

   if (gelf_getehdr(elf, &ehdr) == NULL) {
      //fprintf(stderr,"%s: cannot get elf header of %s.\n", __func__, file);
      goto out_elf_end;
   }

   sec = elf_section_by_name(elf, &ehdr, &shdr, ".symtab", NULL);
   if (sec == NULL) {
      sec = elf_section_by_name(elf, &ehdr, &shdr, ".dynsym", NULL);
      if (sec == NULL) {
         //fprintf(stderr,"%s(%s): cannot get .symtab or .dynsym", __func__, file);
         goto out_elf_end;
      }
   }

   syms = elf_getdata(sec, NULL);
   if (syms == NULL)
      goto out_elf_end;

   sec = elf_getscn(elf, shdr.sh_link);
   if (sec == NULL)
      goto out_elf_end;

   symstrs = elf_getdata(sec, NULL);
   if (symstrs == NULL)
      goto out_elf_end;

   sec_strndx = elf_getscn(elf, ehdr.e_shstrndx);
   if (sec_strndx == NULL)
      goto out_elf_end;

   secstrs = elf_getdata(sec_strndx, NULL);
   if (secstrs == NULL)
      goto out_elf_end;

   nr_syms = shdr.sh_size / shdr.sh_entsize;

   memset(&sym, 0, sizeof(sym));
   if (!kernel) {
      adjust_symbols
               = (ehdr.e_type == ET_EXEC
                        || elf_section_by_name(elf, &ehdr, &shdr, ".gnu.prelink_undo", NULL)
                                 != NULL);
   }
   else
      adjust_symbols = 0;

   elf_symtab__for_each_symbol(syms, nr_syms, idx, sym) {
      struct symb *s;
      const char *elf_name;
      char *demangled;
      uint64_t obj_start;
      int is_label = elf_sym__is_label(&sym);
      const char *section_name;

      if (!is_label && !elf_sym__is_function(&sym))
         continue;

      sec = elf_getscn(elf, sym.st_shndx);
      if (!sec)
         goto out_elf_end;

      gelf_getshdr(sec, &shdr);

      if (is_label && !elf_sec__is_text(&shdr, secstrs))
         continue;

      section_name = elf_sec__name(&shdr, secstrs);
      obj_start = sym.st_value;

      if (adjust_symbols) {
         sym.st_value -= shdr.sh_addr - shdr.sh_offset;
      }

      /*
       * We need to figure out if the object was created from C++ sources
       * DWARF DW_compile_unit has this, but we don't always have access
       * to it...
       */
      elf_name = elf_sym__name(&sym, symstrs);
      demangled = bfd_demangle(NULL, elf_name, DMGL_PARAMS | DMGL_ANSI);
      if (demangled != NULL)
         elf_name = demangled;

      s = symb_new();
      s->hex = sym.st_value;
      s->name = strdup(elf_name);
      symb_insert(s, arr);
      free(demangled);
      nr++;
   }
   symb_synthesize_plt_symbols(arr, elf);
   out_elf_end: elf_end(elf);
   close(fd);

   out_close: g_hash_table_insert(parsed_files, file, arr);
   return;
}
/** END USERLAND **/

