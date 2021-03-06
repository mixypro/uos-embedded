#ifndef __USBDEV_H__
#define __USBDEV_H__

#include <runtime/lib.h>
#include <kernel/uos.h>
#include "usb_const.h"
#include "usb_struct.h"
#include "usbdev_dfl_settings.h"

//
// Настроечные параметры стека USB-устройства.
// Для экономия ОЗУ рекомендуется задавать минимально необходимые значения.
//

// Максимальное количество одновременно задействованных интерфейсов
// (наибольшее из количеств интерфейсов по каждой конфигурации)
#ifndef USBDEV_NB_INTERFACES
#define USBDEV_NB_INTERFACES        1
#endif

// Количество используемых конечных точек
#ifndef USBDEV_NB_ENDPOINTS
#define USBDEV_NB_ENDPOINTS         16
#endif

// Количество конфигураций устройства
#ifndef USBDEV_NB_CONF
#define USBDEV_NB_CONF              1
#endif

// Размер максимальной длины передачи для нулевой конечной точки
#ifndef USBDEV_EP0_MAX_SIZE
#define USBDEV_EP0_MAX_SIZE         8
#endif

// Глубина приёмных очередей по умолчанию
#ifndef USBDEV_DEFAULT_RXQ_DEPTH
#define USBDEV_DEFAULT_RXQ_DEPTH    2
#endif

//
// Направления конечных точек
// Endpoint directions
//
#define USBDEV_DIR_OUT              0
#define USBDEV_DIR_IN               1

//
// Типы транзакций
// Transaction types
//
#define USBDEV_TRANSACTION_OUT      0
#define USBDEV_TRANSACTION_SETUP    1
#define USBDEV_TRANSACTION_IN       2

//
// Состояния устройства
// Device states
//
#define USBDEV_STATE_IDLE           0x00
#define USBDEV_STATE_ATTACHED       0x01
#define USBDEV_STATE_POWERED        0x02
#define USBDEV_STATE_DEFAULT        0x04
#define USBDEV_STATE_ADDRESS        0x08
#define USBDEV_STATE_CONFIGURED     0x10
#define USBDEV_STATE_SUSPENDED      0x20

//
// Состояния конечных точек
// Endpoint states
//
#define EP_STATE_DISABLED           0x000
#define EP_STATE_WAIT_SETUP         0x001
#define EP_STATE_WAIT_OUT           0x002
#define EP_STATE_WAIT_OUT_ACK       0x004
#define EP_STATE_SETUP_DATA_OUT     0x008
#define EP_STATE_WAIT_IN            0x010
#define EP_STATE_WAIT_IN_LAST       0x020
#define EP_STATE_WAIT_IN_ACK        0x040
#define EP_STATE_NACK               0x100
#define EP_STATE_STALL              0x200
#define EP_STATE_FROM_SOF           0x400

#define EP_WAIT_IN_STATES           0x070

//
// Коды возврата для функций-обработчиков событий устройства,
// интерфейса или конечной точки.
// Данные коды сообщают стеку USB, в какое состояние нужно перевести
// конечную точку (в случае функции-обработчика для устройства или 
// интерфейса - конечную точку 0) после возврата из обработчика.
//
#define USBDEV_ACK      0
#define USBDEV_NACK     1
#define USBDEV_STALL    2

struct _usbdev_t;
struct _usbdev_hal_t;
typedef struct _usbdev_t usbdev_t;
typedef struct _usbdev_hal_t usbdev_hal_t;

//
// Функции, вызываемые стеком USBDEV (должны быть реализованы аппаратным драйвером (HAL)
// Functions called by USBDEV stack (to be realized by HAL)
//

// Установка адреса устройства.
// Эта функция должна установить адрес устройства, указанный в параметре addr.
typedef void (*usbdev_set_addr_func_t) (unsigned addr, void *arg);

// Установка атрибутов конечной точки.
// Номер конечной точки указан в параметре ep, направление - в параметре dir.
// Параметр dir может принимать значения USBDEV_DIR_OUT или USBDEV_DIR_IN.
// attr - требуемые атрибуты конечной точки (TODO: описать атрибуты).
// max_size - максимальный размер передачи.
// interval - интервал передачи (имеет значение только для изохронных конечных точек).
typedef void (*usbdev_set_ep_attr_func_t) (unsigned ep, int dir, unsigned attr, int max_size, int interval, void *arg);

// Функция определения имеющегося свободного места в передатчике.
// Данная функция должна возвращать, сколько ещё байт можно выдать в конечную точку с
// номером ep.
typedef int  (*usbdev_in_avail_func_t) (unsigned ep, void *arg);

// Функция установки конечной точки в состояние ожидания приёма пакета от хоста.
// Конечная точка должна принимать все входящие пакеты, как OUT, так и SETUP.
typedef void (*usbdev_ep_wait_out_func_t) (unsigned ep, int ack, void *arg);

// Функция установки конечной точки в состояние ожидания выдачи пакета хосту.
// ep - номер конечной точки.
// pid - PID пакета для выдачи (PID_DATA0 или PID_DATA1).
// data - указатель на буфер с данными пакета.
// size - размер буфера с данными.
// last - признак последнего пакета в текущей передаче
typedef void (*usbdev_ep_wait_in_func_t) (unsigned ep, int pid, const void *data, int size, int last, void *arg);

// Функция установки конечной точки в состояние STALL.
// ep - номер конечной точки.
// dir - направление конечной точки (USBDEV_DIR_OUT или USBDEV_DIR_IN).
typedef void (*usbdev_ep_stall_func_t) (unsigned ep, int dir, void *arg);

// Прототип функции-обработчика запросов, специфичных для класса.
// Обработчик регистрируется в стеке вызовом usbdev_set_class_handler.
typedef int (*usbdev_specific_t) (usbdev_t *u, void *tag, usb_setup_pkt_t *setup_pkt, unsigned char **data, int *size);

typedef void (*usbdev_ack_t) (usbdev_t *u, unsigned ep, int dir, void *tag);

// Структура с функциями, вызываемыми стеком USBDEV.
struct _usbdev_hal_t
{
    usbdev_set_addr_func_t      set_addr;
    usbdev_set_ep_attr_func_t   ep_attr;
    usbdev_ep_wait_out_func_t   ep_wait_out;
    usbdev_ep_wait_in_func_t    ep_wait_in;
    usbdev_ep_stall_func_t      ep_stall;
    usbdev_in_avail_func_t      in_avail;
};

//
// Внутренняя управляющая структура одной конечной точки (сразу для двух направлений
// конечной точки - IN и OUT). Не рекомендуется использовать информацию из данной
// структуры в приложении или в аппаратном драйвере.
// Endpoint control structure
//
typedef struct _ep_out_t {
    int             state;          // Состояние конечной точки
    int             attr;           // Атрибуты конечной точки, взятые из 
                                    // дескриптора конечной точки
    int             max_size;       // Максимальный размер передачи
    int             interval;       // Интервал (для изохронных конечных точек)
    
    usbdev_specific_t   specific_handler;
    void *              specific_tag;
} ep_out_t;

typedef struct _ep_in_t {
    int             state;          // Состояние конечной точки
    int             attr;           // Атрибуты конечной точки, взятые из 
                                    // дескриптора конечной точки
    int             max_size;       // Максимальный размер передачи для конечных точек
    int             interval;       // Интервал (для изохронных конечных точек)
    const uint8_t * ptr;            // Текущий указатель, из которого выдаётся
                                    // очередная порция сообщения
    int             rest_load;      // Количество байтов, которое осталось выдать в
                                    // текущей передаче
    int             rest_ack;       // Количество байтов, которое осталось подтвердить.
                                    // Данное число увеличивается в момент получения ACK от хоста
    int             pid;            // PID для следующей выдачи
    int             shorter_len;    // Признак ответа хосту пакетом меньшей длины, чем запрошено
    
    int 			active;			// Признак активной BULK IN EP

    usbdev_specific_t   specific_handler;
    void *              specific_tag;
} ep_in_t;


typedef struct _iface_ctrl_t {
    usbdev_specific_t   if_specific_handler;
    void *              if_specific_tag;
} iface_ctrl_t;

//
// USB device driver structure
//
struct __attribute__ ((packed)) _usbdev_t
{
    usb_dev_desc_t *        dev_desc;
    usb_qualif_desc_t *     qualif_desc;
    usb_conf_desc_t *       conf_desc [USBDEV_NB_CONF];
    void **                 strings;
    unsigned                cur_conf;
    unsigned                usb_addr;
    
    usbdev_hal_t *          hal;
	void *					hal_arg;
    mutex_t *               hal_lock;
    mem_pool_t *            pool;
    
    ep_out_t                ep_out [USBDEV_NB_ENDPOINTS];
    ep_in_t                 ep_in [USBDEV_NB_ENDPOINTS];
    iface_ctrl_t            iface_ctrl [USBDEV_NB_INTERFACES];
    
    usbdev_specific_t       dev_specific_handler;
    void *                  dev_specific_tag;
    
    usbdev_ack_t            ack_handlers [USBDEV_NB_ENDPOINTS] [2];
    void *                  ack_handler_tags [USBDEV_NB_ENDPOINTS] [2];
    
    int                     first_device_descr;
    
    // Statistics
    unsigned                rx_discards;
    unsigned                rx_req;
    unsigned                rx_bad_req;
    unsigned                rx_bad_trans;
    unsigned                rx_bad_len;
    unsigned                ctrl_failed;
    unsigned                out_of_memory;    
};


//
// Функции обратного вызова. Вызываются аппаратным драйвером.
// Interface for HAL (callbacks). Do not call from user code!
//

// Привязка аппаратного драйвера к стеку. Функция должна вызываться
// в момент инициализации аппаратного драйвера. Функция передаёт в 
// параметре hal в стек структуру с функциями, которые затем будут 
// вызываться стеком.
void usbdevhal_bind (usbdev_t *u, usbdev_hal_t *hal, void *arg, mutex_t *hal_mutex);

// Эту функцию должен вызвать аппаратный драйвер по событию сброса шины USB.
void usbdevhal_reset (usbdev_t *u);

// Эту функцию должен вызвать аппаратный драйвер в случае определения состояния IDLE
// в шине USB.
void usbdevhal_suspend (usbdev_t *u);

// Эту функцию должен вызвать аппаратный драйвер по окончанию выдачи пакета хосту.
// ep - номер конечной точки, size - размер выданного пакета.
void usbdevhal_in_done (usbdev_t *u, unsigned ep, int size);

// Эту функцию должен вызвать аппаратный драйвер по окончанию приёма пакета от хоста.
// ep - номер конечной точки, pid - PID принятого пакета, data - указатель на
// буфер с принятым пакетом, size - размер пакета.
void usbdevhal_out_done (usbdev_t *u, unsigned ep, int trans_type, void *data, int size);

// Эту функцию должен вызвать аппаратный драйвер по приходу SOF.
// Функция подготавливает данные для передачи BULK IN оконечной точкой.
void usbdev_sof_done (usbdev_t *u, unsigned ep_n);

//
// USB device API
//
void usbdev_init (usbdev_t *u, mem_pool_t *pool, const usb_dev_desc_t *dd);
void usbdev_add_config_desc (usbdev_t *u, const void *cd);
void usbdev_set_string_table (usbdev_t *u, const void *st[]);
void usbdev_set_dev_specific_handler (usbdev_t *u, usbdev_specific_t handler, void *tag);
void usbdev_set_iface_specific_handler (usbdev_t *u, unsigned if_n, usbdev_specific_t handler, void *tag);
void usbdev_set_ep_specific_handler (usbdev_t *u, unsigned ep_n, int dir, usbdev_specific_t handler, void *tag);
void usbdev_set_ack_handler (usbdev_t *u, unsigned ep_n, int dir, usbdev_ack_t handler, void *tag);
void usbdev_remove_ack_handler (usbdev_t *u, unsigned ep_n, int dir);
void usbdev_ack_in (usbdev_t *u, unsigned ep_n, const void *data, int size);
void usbdev_set_ack (usbdev_t *u, unsigned ep_n);
int  usbdev_recv (usbdev_t *u, unsigned ep_n, void *data, int size);

// Активация BULK EP
void usbdev_activate_ep(usbdev_t *u, unsigned ep_n);
void usbdev_deactivate_ep(usbdev_t *u, unsigned ep_n);
#endif
