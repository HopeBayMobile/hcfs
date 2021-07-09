/*
 * Copyright (c) 2021 HopeBayTech.
 *
 * This file is part of Tera.
 * See https://github.com/HopeBayMobile for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "zipint.h"


ZIP_EXTERN int
zip_source_seek(zip_source_t *src, zip_int64_t offset, int whence)
{
    zip_source_args_seek_t args;
    
    if (src->source_closed) {
        return -1;
    }
    if (!ZIP_SOURCE_IS_OPEN_READING(src) || (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END)) {
        zip_error_set(&src->error, ZIP_ER_INVAL, 0);
        return -1;
    }

    args.offset = offset;
    args.whence = whence;
    
    return (_zip_source_call(src, &args, sizeof(args), ZIP_SOURCE_SEEK) < 0 ? -1 : 0);
}


zip_int64_t
zip_source_seek_compute_offset(zip_uint64_t offset, zip_uint64_t length, void *data, zip_uint64_t data_length, zip_error_t *error)
{
    zip_int64_t new_offset;
    zip_source_args_seek_t *args = ZIP_SOURCE_GET_ARGS(zip_source_args_seek_t, data, data_length, error);
    
    if (args == NULL) {
        return -1;
    }
    
    switch (args->whence) {
        case SEEK_CUR:
            new_offset = (zip_int64_t)offset + args->offset;
            break;
            
        case SEEK_END:
            new_offset = (zip_int64_t)length + args->offset;
            break;
            
        case SEEK_SET:
            new_offset = args->offset;
            break;
            
        default:
            zip_error_set(error, ZIP_ER_INVAL, 0);
            return -1;
    }
    
    if (new_offset < 0 || (zip_uint64_t)new_offset > length) {
        zip_error_set(error, ZIP_ER_INVAL, 0);
        return -1;
    }
    
    return new_offset;
}
