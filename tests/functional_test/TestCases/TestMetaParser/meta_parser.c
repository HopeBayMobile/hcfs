#include <sys/types.h>
#include <stdint.h>

#include <python2.7/Python.h>

struct hcfs_stat { 
        uint64_t st_dev;    /** ID of device containing file */
        uint64_t st_ino;    /** inode number */
        uint32_t st_mode;   /** Ubuntu x86_64:   uint32_t, */
};

typedef struct hcfs_stat hcfs_statObj;
typedef hcfs_statObj * hcfs_statPtr;

static PyObject* list_external_volume(PyObject* self, PyObject* args) {
	char *fsmgr_path;

	if(!PyArg_ParseTuple(args, "s", &fsmgr_path)) return NULL;

	PyObject *lst = PyList_New(3);
	if (!lst)
    	return NULL;
	PyList_SET_ITEM(lst, 0, Py_BuildValue("(i,s)", 1, "first inode")); 
	PyList_SET_ITEM(lst, 1, Py_BuildValue("(i,s)", 22, "second inode")); 
	PyList_SET_ITEM(lst, 2, Py_BuildValue("(i,s)", 123, fsmgr_path)); 

	return lst;
}
static PyObject* hcfs_parse_meta(PyObject* self, PyObject* args) {
	char *meta_path;
	hcfs_statPtr statPtr = (hcfs_statPtr)malloc(sizeof(hcfs_statObj));
	statPtr->st_dev = 223;
	statPtr->st_ino = 98;
	statPtr->st_mode = 2222222;

	if(!PyArg_ParseTuple(args, "s", &meta_path)) return NULL;

	PyObject *list;

	list = Py_BuildValue("[]");
    PyList_Append(list, (PyObject *) Py_BuildValue("{s:i}", "st_dev", statPtr->st_dev));
	PyList_Append(list, (PyObject *) Py_BuildValue("{s:i}", "st_ino", statPtr->st_ino));
	PyList_Append(list, (PyObject *) Py_BuildValue("{s:i}", "st_mode", statPtr->st_mode));

	return Py_BuildValue("(i,i,i,O)", 1, 2, 22, list);
}

static PyObject* hcfs_list_dir_inorder(PyObject* self, PyObject* args) {
	char *meta_path;
	int offset_bgn, offset_end, limit;

	if(!PyArg_ParseTuple(args, "s(ii)i", &meta_path, &offset_bgn, &offset_end, &limit)) return NULL;

	PyObject *lst = PyList_New(6);
	if (!lst)
    	return NULL;
	PyList_SET_ITEM(lst, 0, Py_BuildValue("(i,s)", 1, "first inode")); 
	PyList_SET_ITEM(lst, 1, Py_BuildValue("(i,s)", 22, "second inode")); 
	PyList_SET_ITEM(lst, 2, Py_BuildValue("(i,s)", 123, meta_path)); 
	PyList_SET_ITEM(lst, 3, Py_BuildValue("(i,s)", offset_bgn, "offset begin")); 
	PyList_SET_ITEM(lst, 4, Py_BuildValue("(i,s)", offset_end, "offset end")); 
	PyList_SET_ITEM(lst, 5, Py_BuildValue("(i,s)", limit, "limit")); 

	return lst;
}

static struct PyMethodDef meta_parser_methods[] = {
    {"list_external_volume", list_external_volume, 1, "Read fsmgr file to get root inode of volumes."},
    {"hcfs_parse_meta", hcfs_parse_meta, 1, "Get file information. It returns metadata of a hcfs meta file."},
    {"hcfs_list_dir_inorder", hcfs_list_dir_inorder, 1, "Get a range of file list inside the directory. Return the inode's childs in dictionary order Wranged by index and limit. e.g. Querying with (index=101, limit=20) returns child 101~120."},
    {NULL, NULL}
};

void initlib(void) {
    Py_InitModule("lib", meta_parser_methods);
}
