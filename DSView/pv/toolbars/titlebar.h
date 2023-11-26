/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2016 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef DSVIEW_PV_TOOLBARS_TITLEBAR_H
#define DSVIEW_PV_TOOLBARS_TITLEBAR_H

#include <QWidget>
 #include "../interface/icallbacks.h"

class QToolButton;
class QHBoxLayout;
class QLabel;

namespace pv {
namespace toolbars {

class TitleBar : public QWidget, public IFontForm
{
    Q_OBJECT

public:
    TitleBar(bool top, QWidget *parent, bool hasClose = false);
    ~TitleBar();
    
    void setTitle(QString title); 
    QString title();

    //IFontForm
    void update_font() override;

private:
    void changeEvent(QEvent *event) override;
    void reStyle();

signals:
    void normalShow();
    void maximizedShow();

public slots:
    void showMaxRestore();
    void setRestoreButton(bool max);
    inline bool IsMoving(){return _moving;}

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
 
    
    QToolButton *_minimizeButton;
    QToolButton *_maximizeButton;
    QToolButton *_closeButton;
    QLabel      *_title;
  
    bool        _moving;
    bool        _isTop;
    bool        _hasClose;
    QPoint      _clickPos;
    QPoint      _oldPos;
    QWidget     *_parent;
};

} // namespace toolbars
} // namespace pv

#endif // DSVIEW_PV_TOOLBARS_TITLEBAR_H
