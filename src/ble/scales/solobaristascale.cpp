#include "solobaristascale.h"

SoloBarristaScale::SoloBarristaScale(ScaleBleTransport* transport, QObject* parent)
    : EurekaPrecisaScale(transport, parent)
{
    m_scaleName = "Solo Barista";
}
