#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>


/* numeric conversion by strtol */
#include <errno.h>
#include <limits.h>
 

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "glb_config.h"
#include "glb_utils.h"

int compare(const char *a, const char *b)
{
    int i;

    for (i = 0; i < GLB_ID_MATRIX_DEPTH; i++) {
        if (a[i] > b[i]) return glb_MORE;
        else if (a[i] < b[i]) return glb_LESS;
    }
    
    return glb_EQUALS;
}

int inc_id(char *id)
{
    size_t i;

    i = GLB_ID_MATRIX_DEPTH - 1;
    begin:
    
    if (id[i] == '9') { id[i] = 'A'; return glb_OK;}
    if (id[i] == 'Z') { id[i] = 'a'; return glb_OK;}
    if (id[i] == 'z') { id[i--] = '0'; goto begin; }
    id[i]++;

    return glb_OK;
}

const char *max_id(const char *a, const char *b)
{
    return (compare(a, b) == glb_MORE) ? a : b;
}

const char *min_id(const char *a, const char *b)
{
    return compare(a, b) == glb_LESS ? a : b;
}


static int 
glb_mkdir(const char *path, mode_t mode)
{
    struct stat st;
    int ret = glb_OK;

    if (stat(path, &st) != 0) {

        /* directory does not exist */
        if (mkdir(path, mode) != 0)
            ret = glb_FAIL;
    }
    else if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        ret = glb_FAIL;
    }

    return ret;
}


/**
 * glb_mkpath - ensure all directories in path exist
 */
int glb_mkpath(const char *path, mode_t mode)
{
    char *p;
    char *sep;
    int  ret;
    char *path_buf = strdup(path);

    ret = glb_OK;
    p = path_buf;

    while (ret == glb_OK && 
           (sep = strchr(p, '/')) != 0) {
	if (sep != p) {
            *sep = '\0';
            ret = glb_mkdir(path_buf, mode);
            *sep = '/';
        }
        p = sep + 1;
    }

    /* in case no final dir separator is present at the end */
    if (ret == glb_OK)
        ret = glb_mkdir(path, mode);

    free(path_buf);

    return ret;
}

int glb_write_file(const char *path, const char *filename, 
		   void *buf, size_t buf_size)
{
    char name_buf[GLB_TEMP_BUF_SIZE];
    int fd;

    sprintf(name_buf, "%s/%s", path, filename);

    /* write textual content */
    fd = open((const char*)name_buf,  
	      O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if (fd < 0) return glb_IO_FAIL;

    write(fd, buf, buf_size);
    close(fd);

    return glb_OK;
}


extern int 
glb_get_conc_prefix(const char *name,
		    size_t name_size,
			char *prefix)
{
    char *buf;
    int digit_size = GLB_CONC_PREFIX_DIGIT_SIZE;
    int pref_size;
    int i = 0;

    if (!name_size) return glb_FAIL;

    for (i = 0; i <= GLB_CONC_PREFIX_SIZE; i += digit_size){
	memcpy(prefix, name, i);
	if (i > name_size) break;
    }

    prefix[i - digit_size] = '\0';

    return glb_OK;
}


int glb_make_id_path(char *buf,
		     const char *path,
		     const char *id, 
		     const char *filename)
{
    char *curr_buf = buf;
    size_t path_size;
    int i;

    sprintf(curr_buf, "%s", path);

    path_size = strlen(curr_buf);
    curr_buf += path_size;

    /* treat each digit in the id as a folder */
    for (i = 0; i < GLB_ID_MATRIX_DEPTH; i++) {
	*curr_buf =  '/';
	curr_buf++;
	*curr_buf = id[i];
	curr_buf++;
    }

    if (filename) {
	sprintf(curr_buf, "/%s", filename);
    }

    return glb_OK;
}



/**
 * read XML attr value, allocate memory and copy 
 */
extern int
glb_copy_xmlattr(xmlNode    *input_node,
		const char *attr_name,
		char       **result,
		size_t     *result_size)
{
     char *value;
     char *val_copy;
     int ret;

     value = (char*)xmlGetProp(input_node,  (const xmlChar *)attr_name);
     if (!value) return glb_FAIL;
     
     /* overwrite the previous value if any */
     if ((*result))
	 free((*result));

     (*result_size) = strlen(value);
     val_copy = malloc((*result_size) + 1);
     if (!val_copy) {
	 xmlFree(value);
	 return glb_NOMEM;
     }

     strcpy(val_copy, value);
     xmlFree(value);

     /*printf("VALUE: %s\n\n", val_copy);*/

     (*result) = val_copy;

     return glb_OK;
}


/**
 * read XML attr numeric value
 */
extern int
glb_get_xmlattr_num(xmlNode *input_node,
		    const char *attr_name,
		    long *result)
{
    char *val;
    long numval;
    char *invalid_num_char = NULL;
    int ret = glb_OK;

    val = (char*)xmlGetProp(input_node,  (const xmlChar *)attr_name);
    if (!val) return glb_FAIL;
	    
    errno = 0;
    numval = strtol((const char*)val, &invalid_num_char, GLB_NUM_ENCODE_BASE);
    
    /* check for various numeric decoding errors */
    if ((errno == ERANGE && (numval == LONG_MAX || numval == LONG_MIN))
	|| (errno != 0 && numval == 0)) {
	perror("strtol");
	ret = glb_FAIL;
	goto final;
    }
    
    if (invalid_num_char == val) {
	fprintf(stderr, "  -- No digits were found in \"%s\"\n", val);
	ret = glb_FAIL;
	goto final;
    }
    
    *result = numval;

final:
    if (val)
	xmlFree(val);
   
    return ret;
}

/**
 */
extern int
glb_parse_num(const char *val,
	      long *result)
{
    long numval;
    char *invalid_num_char = NULL;
    int ret = glb_OK;

    if (!val) return glb_FAIL;
	    
    errno = 0;
    numval = strtol(val, &invalid_num_char, GLB_NUM_ENCODE_BASE);
    
    /* check for various numeric decoding errors */
    if ((errno == ERANGE && (numval == LONG_MAX || numval == LONG_MIN))
	|| (errno != 0 && numval == 0)) {
	perror("strtol");
	ret = glb_FAIL;
	goto final;
    }
    
    if (invalid_num_char == val) {
	fprintf(stderr, "  -- No digits were found in \"%s\"\n", val);
	ret = glb_FAIL;
	goto final;
    }
    
    *result = numval;

final:
   
    return ret;
}


extern int 
glb_remove_nonprintables(char *data)
{
    unsigned char *c;
    c = (unsigned char*)data;

    while (*c) {
	if (*c < 32) {
	    *c = ' ';
	}
	if (*c == '\"') *c = ' ';
	if (*c == '\'') *c = ' ';
	if (*c == '&') *c = ' ';
	if (*c == '\\') *c = ' '; 
	c++;
    }

    return glb_OK;
}


static int
glbData_del(struct glbData *self)
{
    if (!self) return glb_OK;
    free(self);
    return glb_OK;
}


static int
glbData_reset(struct glbData *self)
{

    if (self->id) free(self->id);
    if (self->ticket) free(self->ticket);
    if (self->local_id) free(self->local_id);
    if (self->local_path) free(self->local_path);

    if (self->spec) free(self->spec);
    if (self->obj) free(self->obj);

    if (self->text) free(self->text);
    if (self->interp) free(self->interp);

    if (self->topics) free(self->topics);
    if (self->index) free(self->index);
    if (self->query) free(self->query);

    if (self->reply) free(self->reply);

    if (self->metadata) free(self->metadata);
    if (self->results) free(self->results);

    if (self->control_msg) free(self->control_msg);

    memset(self, 0, sizeof(struct glbData));

    self->del = glbData_del;
    self->reset = glbData_reset;
  
  return glb_OK;
}

extern int 
glbData_new(struct glbData **data)
{
    struct glbData *self;

    self = malloc(sizeof(struct glbData));
    if (!self) return glb_NOMEM;

    memset(self, 0, sizeof(struct glbData));

    self->del = glbData_del;
    self->reset = glbData_reset;

    *data = self;

    return glb_OK;

}
