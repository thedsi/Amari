#ifndef GUARD_MainWnd_H_INCLUDED
#define GUARD_MainWnd_H_INCLUDED
#pragma once

#include <QMainWindow>
#include <QFuture>
#include <vector>
#include <random>

QT_BEGIN_NAMESPACE
class QLabel;
class QTimer;
class QCheckBox;
QT_END_NAMESPACE

class MainWnd 
    : public QMainWindow
{
public:
    MainWnd(QWidget* parent = nullptr);
     
    enum Params
    {
        S_t,
        S_h,
        S_k,
        S_K,
        S_m,
        S_M,
        S_s,
        MaxParam
    };   

protected:
    bool eventFilter(QObject*, QEvent*) override;

private:
 
    void WrapV(double* field);
    void WrapH(double* field);
    void ResetField();
    void Stimulate();
    void MakeKernel(double sd, std::vector<double>& oKernel);
    void ApplyKernel(const std::vector<double>& kernel, double* output);
    void UpdateIterationLabel();

    QCheckBox* _autoCenter;
    QLabel* _iterationLabel;
    int _iteration;
    int _width;
    int _height;
    int _realW;
    int _realH;
    int _rboundH;
    int _rboundW;
    int _maxRadius;
    double _param[MaxParam];
    
    std::vector<QFuture<void>> _futures;
    
    std::mt19937 _random;
    
    std::vector<double> _activity;
    std::vector<double> _stimulus;
    std::vector<double> _inhibition;
    std::vector<double> _excitement;

    std::vector<double> _kernelExcitement;
    std::vector<double> _kernelInhibition;

    QTimer* _timer;
};

#endif//GUARD
