# Tunuino Console TX queue

A fixed-size 256-byte FIFO buffers complete protocol frames. `tunuino_tx_queue_write` is an atomic-frame enqueue callback for `tunuino_console_tx_init`. A frame is rejected without modifying queue contents when there is insufficient space.

The queue stores bytes, does not allocate heap memory and tracks dropped frames. `tunuino_tx_queue_pop` is intended for a future UART TX-ready interrupt.

**Concurrency warning:** this initial queue is not thread-safe or interrupt-safe. If producer and consumer run in different interrupt contexts, protect head/tail/count updates with a correctly scoped critical section or replace with a proven single-producer/single-consumer design. Never access it simultaneously without synchronization.

**Hardware status:** no USART peripheral, GPIO alternate function, NVIC interrupt or DMA is configured. UART selection depends on verified PCB pin mapping. The queue does not enable injector or ignition outputs.
