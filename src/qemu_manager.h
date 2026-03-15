#ifndef QEMU_MANAGER_H
#define QEMU_MANAGER_H

class QEMUManager {
public:
    QEMUManager();
    ~QEMUManager();

    void start();
    void stop();
    void configureHardwareAcceleration();

private:
    void initializeQEMU();
    void cleanupQEMU();
};

#endif // QEMU_MANAGER_H
