// No complicated synchronization
// Writer writes whenever he can
// Reader should probably always read as much as he can
// Can define a low limit for reader
// If avaiable data is less than low limit, reader sets a bool, leaves the critical section, waits some time
// Else reader copies data, sends over network
// No semaphores or locks except