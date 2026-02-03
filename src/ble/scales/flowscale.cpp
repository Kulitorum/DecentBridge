#include "flowscale.h"
#include "../../core/settings.h"
#include <QDebug>
#include <QDateTime>

FlowScale::FlowScale(QObject* parent)
    : ScaleDevice(parent)
{
    // FlowScale is always "connected" since it's virtual
    setConnected(true);
}

void FlowScale::connectToDevice(const QBluetoothDeviceInfo& device) {
    Q_UNUSED(device);
    // No-op - FlowScale doesn't use BLE
}

void FlowScale::setSettings(Settings* settings) {
    m_settings = settings;
}

void FlowScale::tare() {
    m_accumulatedWeight = 0.0;
    setWeight(0.0);
    setFlowRate(0.0);
}

void FlowScale::addFlowSample(double flowRate, double deltaTime) {
    // Integrate flow: weight += flow_rate * time * calibration
    // flowRate is in mL/s, deltaTime is in seconds
    // Calibration factor (default 1.29 based on testing)
    double calibrationFactor = 1.29;
    if (deltaTime > 0 && deltaTime < 1.0) {  // Sanity check
        // Track raw (uncalibrated) integral for calibration purposes
        double rawIncrement = flowRate * deltaTime;
        m_rawFlowIntegral += rawIncrement;
        emit rawFlowIntegralChanged();

        // Apply calibration for displayed weight
        m_accumulatedWeight += rawIncrement * calibrationFactor;
        setWeight(m_accumulatedWeight);
        setFlowRate(flowRate * calibrationFactor);
    }
}

void FlowScale::reset() {
    m_accumulatedWeight = 0.0;
    setWeight(0.0);
    setFlowRate(0.0);
}

void FlowScale::resetWeight() {
    tare();  // Same as tare - reset accumulated weight
}

void FlowScale::setSimulatedWeight(double weight) {
    // Directly set weight from simulator (bypasses flow integration)
    m_accumulatedWeight = weight;
    setWeight(weight);

    // Estimate flow rate from weight change (for display)
    // This is approximate but good enough for simulation
    static double lastWeight = 0.0;
    static qint64 lastTime = 0;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (lastTime > 0 && now > lastTime) {
        double dt = (now - lastTime) / 1000.0;
        if (dt > 0 && dt < 1.0) {
            double flowRate = (weight - lastWeight) / dt;
            setFlowRate(qMax(0.0, flowRate));
        }
    }
    lastWeight = weight;
    lastTime = now;
}
