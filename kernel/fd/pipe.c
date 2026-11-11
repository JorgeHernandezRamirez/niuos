#include "pipe.h"
#include "../mm/pmm.h"
#include "../lib/queue.h"
#include "../drivers/screen.h"
#include "../sched/scheduler.h"

extern void schedule_kernel(void);

uint8_t _schedule(){
    scheduler_current_task()->status=STATUS_TASK_WAITING_PIPE;
    schedule_kernel(); /* duerme aqui; retoma justo aca mismo cuando tty_awake_process() la despierte */  
}

int _pipe_no_operation(fd_struct *fd, char *buf, int bytes){
    return -1;
}

void _awake(queue_t *queue){
    uint16_t *pid = (uint16_t *)dequeue(queue);
    while(pid != NULL){
        scheduler_awake(*pid);
        pid = (uint16_t *)dequeue(queue);
    }
}

int pipe_read(fd_struct *fd, char *buf, int bytes){
    fd_pipe_struct *pipe = (fd_pipe_struct *)fd;
    int i = 0;
    while(pipe->pos_reader == pipe->pos_writer){
        if(pipe->num_writers == 0){ //EOF: buffer vacio y ya nadie puede escribir
            return 0;
        }
        enqueue(pipe->readers, (void *)&scheduler_current_task()->pid);
        _awake(pipe->writers);
        _schedule();
    }
    while(i < bytes && pipe->pos_reader != pipe->pos_writer){
        buf[i++] = pipe->buffer[pipe->pos_reader];
        pipe->pos_reader = (pipe->pos_reader + 1) % PIPE_BUFFER_SIZE;
    }
    _awake(pipe->writers);
    return i;
}

static int _pipe_write_byte(fd_pipe_struct *pipe, char c){
    while((pipe->pos_writer + 1) % PIPE_BUFFER_SIZE == pipe->pos_reader){
        if(pipe->num_readers == 0){ //nadie va a vaciar el buffer, no tiene sentido dormirse
            return -1;
        }
        enqueue(pipe->writers, (void *)&scheduler_current_task()->pid);
        _awake(pipe->readers);
        _schedule();
    }
    pipe->buffer[pipe->pos_writer] = c;
    pipe->pos_writer = (pipe->pos_writer + 1) % PIPE_BUFFER_SIZE;
    return 0;
}

int pipe_write(fd_struct *fd, char *buf, int bytes){
    fd_pipe_struct *pipe = (fd_pipe_struct *)fd;
    if(pipe->num_readers == 0){ //EPIPE: el otro extremo ya no existe
        return -1;
    }
    for(int i = 0; i < bytes; i++){
        if(_pipe_write_byte(pipe, buf[i]) < 0){
            _awake(pipe->readers);
            return i == 0 ? -1 : i; //los lectores se fueron a mitad de la escritura
        }
    }
    _awake(pipe->readers);
    return bytes;
}

void pipe_copy(fd_struct **fd_dest, fd_struct **fd_src){ //el pipe en si no se comparte directamente, solo via reader/writer
    *fd_dest = *fd_src;
}

int pipe_close(fd_struct *fd){
    fd_pipe_struct *pipe = (fd_pipe_struct *)fd;
    _awake(pipe->readers);
    _awake(pipe->writers);
    //TODO: free(pipe) cuando num_readers == 0 && num_writers == 0
    return 0;
}

fd_pipe_struct *pipe_new(){
    fd_pipe_struct *value = (fd_pipe_struct *)vmalloc(sizeof(fd_pipe_struct));
    value->write = pipe_write;
    value->read = pipe_read;
    value->close = pipe_close;
    value->copy = pipe_copy;
    value->pos_writer = 0;
    value->pos_reader = 0;
    value->writers = new_queue();
    value->readers = new_queue();
    value->num_readers = 0;
    value->num_writers = 0;
    return value;
}

int _pipe_reader_read(fd_struct *fd, char *buf, int bytes){
    fd_pipe_struct_reader *reader = (fd_pipe_struct_reader *)fd;
    return pipe_read((fd_struct *)reader->pipe, buf, bytes);
}

int _pipe_writer_write(fd_struct *fd, char *buf, int bytes){
    fd_pipe_struct_writer *writer = (fd_pipe_struct_writer *)fd;
    return pipe_write((fd_struct *)writer->pipe, buf, bytes);
}

int _pipe_reader_close(fd_struct *fd){
    fd_pipe_struct_reader *reader = (fd_pipe_struct_reader *)fd;
    reader->pipe->num_readers--;
    return pipe_close((fd_struct *)reader->pipe);
}

void _pipe_reader_copy(fd_struct **fd_dest, fd_struct **fd_src){ //otra tarea (fork) pasa a tener este extremo abierto
    fd_pipe_struct_reader *reader = (fd_pipe_struct_reader *)*fd_src;
    reader->pipe->num_readers++;
    *fd_dest = *fd_src;
}

void _pipe_writer_copy(fd_struct **fd_dest, fd_struct **fd_src){
    fd_pipe_struct_writer *writer = (fd_pipe_struct_writer *)*fd_src;
    writer->pipe->num_writers++;
    *fd_dest = *fd_src;
}

int _pipe_writer_close(fd_struct *fd){
    fd_pipe_struct_writer *writer = (fd_pipe_struct_writer *)fd;
    writer->pipe->num_writers--;
    return pipe_close((fd_struct *)writer->pipe);
}

fd_pipe_struct_reader *pipe_new_reader(fd_pipe_struct *pipe){
    fd_pipe_struct_reader *value = (fd_pipe_struct_reader *)vmalloc(sizeof(fd_pipe_struct_reader));
    value->pipe = pipe;
    value->read = _pipe_reader_read;
    value->write = _pipe_no_operation;
    value->close = _pipe_reader_close;
    value->copy = _pipe_reader_copy;
    value->pipe->num_readers++;
    return value;
}

fd_pipe_struct_writer *pipe_new_writer(fd_pipe_struct *pipe){
    fd_pipe_struct_writer *value = (fd_pipe_struct_writer *)vmalloc(sizeof(fd_pipe_struct_writer));
    value->pipe = pipe;
    value->read = _pipe_no_operation;
    value->write = _pipe_writer_write;
    value->close = _pipe_writer_close;
    value->copy = _pipe_writer_copy;
    value->pipe->num_writers++;
    return value;
}