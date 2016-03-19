#include "Amari.h"
#include <QApplication>
#include <QDockWidget>
#include <QFormLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPainter>
#include <QPushButton>
#include <QCheckBox>
#include <QMouseEvent>
#include <QTimer>
#include <QLabel>
#include <QThreadPool>
#include <QtConcurrentRun>
#include <limits>
#include <cmath>

#ifdef STATIC_RELEASE
#include <QtPlugin>
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#endif

#define QS QStringLiteral
using std::size_t;


int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    MainWnd main;
    main.show();
    app.exec();
}


MainWnd::MainWnd(QWidget* parent)
    : QMainWindow(parent)
    , _random(std::random_device()())
{
    int maxThreads = QThreadPool::globalInstance()->maxThreadCount();
    if (maxThreads < 1)
        maxThreads = 1;
    _futures.resize(maxThreads);

    setWindowTitle(QS("Генератор лабиринтов"));

    QWidget* display;
    setCentralWidget(display = new QWidget);
    display->installEventFilter(this);

    QDockWidget* dock;
    QWidget* settings;
    QFormLayout* laySettings;
    QSpinBox* settingWidth;
    QSpinBox* settingHeight;

    const double minD = std::numeric_limits<double>::lowest();
    const double maxD = std::numeric_limits<double>::max();
    struct MetaParam
    {
        QString name;
        double defVal;
        double minVal;
        double maxVal;
        double step;
    } meta[MaxParam];

    meta[S_h] = { QS("h"), -0.1,    minD, maxD, 0.001 };
    meta[S_k] = { QS("k"), 0.05,    0.01, maxD, 0.001 };
    meta[S_K] = { QS("K"), 0.125,   minD, maxD, 0.001 };
    meta[S_m] = { QS("m"), 0.025,   0.01, maxD, 0.001 };
    meta[S_M] = { QS("M"), 0.065,   minD, maxD, 0.001 };
    meta[S_s] = { QS("S"), 3.0,      0.5,  4.0, 0.5   };
    meta[S_t] = { QS("t"), 0.0,      0.0, maxD, 0.01  };
    
    _width = 500;
    _height = 500;

    QDoubleSpinBox* setting[MaxParam];
    addDockWidget(Qt::LeftDockWidgetArea, dock = new QDockWidget(QS("Параметры")));
    dock->setWidget(settings = new QWidget);
    dock->setFeatures(dock->DockWidgetMovable | dock->DockWidgetFloatable);
    settings->setLayout(laySettings = new QFormLayout);
    laySettings->addRow(_iterationLabel = new QLabel);
    laySettings->addRow(QS("Ширина"), settingWidth = new QSpinBox);
    laySettings->addRow(QS("Высота"), settingHeight = new QSpinBox);
    settingWidth->setRange(1, 5000);
    settingHeight->setRange(1, 5000);
    settingWidth->setValue(_width);
    settingHeight->setValue(_height);
    connect(settingWidth, (void(QSpinBox::*)(int))&QSpinBox::valueChanged, [this](int val) 
    {
        _width = val;
        ResetField();
    });
    connect(settingHeight, (void(QSpinBox::*)(int))&QSpinBox::valueChanged, [this](int val)
    {
        _height = val;
        ResetField();
    });
    for (size_t i = 0; i < MaxParam; ++i)
    {
        laySettings->addRow(meta[i].name, setting[i] = new QDoubleSpinBox);
        setting[i]->setRange(meta[i].minVal, meta[i].maxVal);
        setting[i]->setSingleStep(meta[i].step);
        setting[i]->setDecimals(3);
        setting[i]->setValue(meta[i].defVal);
        _param[i] = meta[i].defVal;
        connect(setting[i], (void(QDoubleSpinBox::*)(double))&QDoubleSpinBox::valueChanged, [this, i](double val)
        {
            _param[i] = val;
            ResetField();
        });
    }
    QPushButton *buttonReset;
    laySettings->addRow(_autoCenter = new QCheckBox(QS("Точка в центре")));
    _autoCenter->setChecked(true);
    connect(_autoCenter, &QCheckBox::stateChanged, this, &MainWnd::ResetField);

    laySettings->addRow(buttonReset = new QPushButton(QS("Очистить поле")));
    connect(buttonReset, &QPushButton::clicked, this, &MainWnd::ResetField);

    _timer = new QTimer(this);
    connect(_timer, &QTimer::timeout, [this]
    {
        Stimulate();
        centralWidget()->update();
    });

    ResetField();
    dock->setMaximumWidth(200);
    resize(_width + dock->sizeHint().width(), _height);
}

bool MainWnd::eventFilter(QObject* obj, QEvent *evt)
{
    QWidget *display = centralWidget();
    if (obj == display && evt->type() == QEvent::Paint)
    {
        QPainter painter(display);
        painter.setPen(Qt::white);
        QRect r = display->rect();
        painter.fillRect(display->rect(), Qt::black);
        for (int i = _maxRadius; i < _rboundH; ++i)
            for (int j = _maxRadius; j < _rboundW; ++j)
                if (_activity[i * _realW + j] > 0.0)
                    for (int x = j - _maxRadius; x < r.width(); x += _width)
                        for (int y = i - _maxRadius; y < r.height(); y += _height)
                            painter.drawPoint(x, y);
        return true;
    }
    else if (obj == display && evt->type() == QEvent::MouseButtonRelease)
    {
        QMouseEvent* me = static_cast<QMouseEvent*>(evt);
        if (me->button() == Qt::LeftButton)
        {
            _activity[(me->y() % _height + _maxRadius) * _realW + me->x() % _width  + _maxRadius] = 1.0;
            display->update();
            _timer->start();
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, evt);
}

void MainWnd::WrapV(double* field)
{
    for (int j = _maxRadius; j < _rboundW; ++j)
    {
        for (int i = 0; i < _maxRadius; ++i) // Верхняя
            field[i * _realW + j] = field[(i + _height) * _realW + j];
        for (int i = _rboundH; i < _realH; ++i) // Нижняя
            field[i * _realW + j] = field[(i - _height) * _realW + j];
    }
}

void MainWnd::WrapH(double* field)
{
    for (int i = _maxRadius; i < _rboundH; ++i)
    {
        for (int j = 0; j < _maxRadius; ++j) // Левая
            field[i * _realW + j] = field[i * _realW + j + _width];
        for (int j = _rboundW; j < _realW; ++j) // Правая
            field[i * _realW + j] = field[i * _realW + j - _width];
    }
}

void MainWnd::ResetField()
{
    MakeKernel(1.0 / std::sqrt(2.0 * _param[S_k]), _kernelExcitement);
    MakeKernel(1.0 / std::sqrt(2.0 * _param[S_m]), _kernelInhibition);
    _maxRadius = (int)std::max(_kernelExcitement.size(), _kernelInhibition.size()) - 1;
    if (_width < 2 * _maxRadius)
        _width = 2 * _maxRadius;
    if (_height < 2 * _maxRadius)
        _height = 2 * _maxRadius;
    _realW = _width + _maxRadius * 2;
    _realH = _height + _maxRadius * 2;
    _rboundH  = _maxRadius + _height;
    _rboundW = _maxRadius + _width;
    const size_t count = _realW * _realH;
    _activity.assign(count, _param[S_h]);
    _inhibition.resize(count * 2);
    _excitement.resize(count * 2);
    _stimulus.resize(count);

    std::uniform_real_distribution<> dis;
    for (double& d : _stimulus)
        d = -_param[S_h] * dis(_random);
    
    _timer->stop();
    _timer->setInterval((int)(_param[S_t] * 1000.0));
    if (_autoCenter->isChecked())
    {
        _activity[(count + _realW) / 2] = 100.0;
        _timer->start();
    }

    _iteration = 0;

    UpdateIterationLabel();

    centralWidget()->update();   
}

void MainWnd::Stimulate()
{
    WrapH(_activity.data());
    ApplyKernel(_kernelExcitement, _excitement.data());
    ApplyKernel(_kernelInhibition, _inhibition.data());
    const double Pi = 3.14159265358979323846;
    const double kk = _param[S_K] * Pi / _param[S_k];
    const double mm = _param[S_M] * Pi / _param[S_m];
    for (size_t i = 0; i < _activity.size(); ++i)
        _activity[i] = _param[S_h] + kk * _excitement[i] - mm * _inhibition[i] + _stimulus[i];
    ++_iteration;
    UpdateIterationLabel();
}


void MainWnd::MakeKernel(double sd, std::vector<double>& kernel)
{
    kernel.resize((size_t)(sd * _param[S_s] + 1.5));
    const double v0 = -0.5 / (sd * sd);
    kernel[0] = 1.0;
    double sum = 0.0;
    for (size_t i = 1; i < kernel.size(); ++i)
        sum += (kernel[i] = std::exp((i * i) * v0));   
    sum = 1.0 / (2.0 * sum + 1.0);
    for (double& d : kernel)
        d *= sum;
}

void MainWnd::ApplyKernel(const std::vector<double>& kernel, double* output)
{
    const int numThreads = (int)_futures.size();
    const int radius = (int)kernel.size() - 1;
    double* o1 = output + _activity.size();
    
    // Горизонтальный проход
    const int deltaH = ((_height + numThreads - 1) / numThreads);
    for (int t = 0; t < numThreads; ++t)
    {
        int start = _maxRadius + t * deltaH;
        int end = std::min(_maxRadius + (t + 1) * deltaH, _rboundH);
        _futures[t] = QtConcurrent::run([=]
        {
            for (int i = start; i < end; ++i)
            {
                for (int j = _maxRadius; j < _rboundW; ++j)
                {
                    double result = 0.0;
                    for (int k = -radius; k <= radius; ++k)
                        if (_activity[i * _realW + j + k] > 0.0)
                            result += kernel[std::abs(k)];
                    o1[i * _realW + j] = result;
                }
            }
        });
    }
    for (int t = 0; t < numThreads; ++t)
        _futures[t].waitForFinished();

    WrapV(o1);

    // Вертикальный проход
    const int deltaV = ((_width + numThreads - 1) / numThreads);
    for (int t = 0; t < numThreads; ++t)
    {
        int start = _maxRadius + t * deltaV;
        int end = std::min(_maxRadius + (t + 1) * deltaV, _rboundW);
        _futures[t] = QtConcurrent::run([=]
        {
            for (int i = start; i < end; ++i)
            {
                for (int j = _maxRadius; j < _rboundH; ++j)
                {
                    double result = 0.0;
                    for (int k = -radius; k <= radius; ++k)
                        result += kernel[std::abs(k)] * o1[(j + k) * _realW + i];
                    output[j * _realW + i] = result;
                }
            }
        });
    }
    for (int t = 0; t < numThreads; ++t)
        _futures[t].waitForFinished();
}

void MainWnd::UpdateIterationLabel()
{
    _iterationLabel->setText(QS("Итерация: %1").arg(_iteration));
}
