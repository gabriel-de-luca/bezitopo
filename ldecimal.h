/******************************************************/
/*                                                    */
/* ldecimal.h - lossless decimal representation       */
/*                                                    */
/******************************************************/
/* Copyright 2015,2017,2020 Pierre Abbat.
 * This file is part of Bezitopo.
 *
 * Bezitopo is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Bezitopo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License and Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * and Lesser General Public License along with Bezitopo. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <string>

std::string ldecimal(double x,double toler=0,bool noexp=false);
/* Returns the shortest decimal representation necessary for
 * the double read back in to be equal to the double written.
 * If toler>0, returns the shortest representation of a number
 * that is within toler of x.
 */
