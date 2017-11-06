/******************************************************/
/*                                                    */
/* cidialog.h - contour interval dialog               */
/*                                                    */
/******************************************************/
/* Copyright 2017 Pierre Abbat.
 * This file is part of Bezitopo.
 * 
 * Bezitopo is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Bezitopo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Bezitopo. If not, see <http://www.gnu.org/licenses/>.
 */
#include <vector>
#include <QDialog>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QGridLayout>
#include "contour.h"

#define MININTERVAL 0.03
#define MAXINTERVAL 30
// This allows contour intervals from 50 mm to 20 m or from 0.1 ft to 50 ft.

class ContourIntervalDialog: public QDialog
{
public:
  ContourIntervalDialog(QWidget *parent=0);
  void set(ContourInterval *ci,Measure meas);
private:
  ContourInterval *contourInterval;
  QLabel *currentInterval;
  QComboBox *comboBox;
  QPushButton *okButton,*cancelButton;
  QGridLayout *gridLayout;
  std::vector<ContourInterval> ciList;
};
