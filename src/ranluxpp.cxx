/*************************************************************************
 * Copyright (C) 2018,  Alexei Sibidanov                                 *
 * All rights reserved.                                                  *
 *                                                                       *
 * This file is part of the RANLUX++ random number generator.            *
 *                                                                       *
 * RANLUX++ is free software: you can redistribute it and/or modify it   *
 * under the terms of the GNU Lesser General Public License as published *
 * by the Free Software Foundation, either version 3 of the License, or  *
 * (at your option) any later version.                                   *
 *                                                                       *
 * RANLUX++ is distributed in the hope that it will be useful, but       *
 * WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 * Lesser General Public License for more details.                       *
 *                                                                       *
 * You should have received a copy of the GNU Lesser General Public      *
 * License along with this program.  If not, see                         *
 * <http://www.gnu.org/licenses/>.                                       *
 *************************************************************************/

#include "ranluxpp.h"
#include "mulmod.h"
#include <stdio.h>

// modular exponentiation:
// x <- x^n mod (2^576 - 2^240 + 1)
void powmod(uint64_t *x, unsigned long int n){
  uint64_t res[9];
  res[0] = 1;
  for(int i=1;i<9;i++) res[i] = 0;
  while(n){
    if(n&1) mul9x9mod(res, x);
    n >>= 1;
    if(!n) break;
    mul9x9mod(x, x);
  }
  for(int i=0;i<9;i++) x[i] = res[i];
}

const uint64_t *ranluxpp::geta(){
  static const uint64_t
    a[9] = {0x0000000000000001UL, 0x0000000000000000UL, 0x0000000000000000UL,
	    0xffff000001000000UL, 0xffffffffffffffffUL, 0xffffffffffffffffUL,
	    0xffffffffffffffffUL, 0xffffffffffffffffUL, 0xfffffeffffffffffUL};
  return a;
}

ranluxpp::ranluxpp(uint64_t seed, uint64_t p) : _dpos(11), _fpos(24) {
  _x[0] = 1;
  for(int i=1;i<9;i++) _x[i] = 0;
  for(int i=0;i<9;i++) _A[i] = geta()[i];
  powmod(_A, p);
  init(seed);
}

// the core of LCG -- modular mulitplication
void ranluxpp::nextstate(){
  mul9x9mod(_x,_A);
}
  
void ranluxpp::nextfloats() {
  nextstate(); unpackfloats((float*)_floats); _fpos = 0;
}
  
void ranluxpp::nextdoubles() {
  nextstate(); unpackdoubles((double*)_doubles); _dpos = 0;
}
  
// unpack state into single precision format
void ranluxpp::unpackfloats(float *a) {
  const uint32_t m = 0xffffff;
  const float sc = 1.0f/0x1p24f;
  for(int i=0;i<3;i++){
    float *f = a + 8*i;
    uint64_t *t = _x + i*3;
    f[0] = sc * (int32_t)(m & t[0]);
    f[1] = sc * (int32_t)(m & ((t[0]>>24)));
    f[2] = sc * (int32_t)(m & ((t[0]>>48)|(t[1]<<16)));
    f[3] = sc * (int32_t)(m & ((t[1]>>8)));
    f[4] = sc * (int32_t)(m & ((t[1]>>32)));
    f[5] = sc * (int32_t)(m & ((t[1]>>56)|(t[2]<<8)));
    f[6] = sc * (int32_t)(m & ((t[2]>>16)));
    f[7] = sc * (int32_t)(m & ((t[2]>>40)));
  }
}

// unpack state into double precision format
// 52 bits out of possible 53 bits are random
void ranluxpp::unpackdoubles(double *d) {
  const uint64_t
    one = 0x3ff0000000000000, // exponent
    m   = 0x000fffffffffffff; // mantissa
  uint64_t *id = (uint64_t*)d;
  id[ 0] = one | (m & _x[0]);
  id[ 1] = one | (m & ((_x[0]>>52)|(_x[1]<<12)));
  id[ 2] = one | (m & ((_x[1]>>40)|(_x[2]<<24)));
  id[ 3] = one | (m & ((_x[2]>>28)|(_x[3]<<36)));
  id[ 4] = one | (m & ((_x[3]>>16)|(_x[4]<<48)));
  id[ 5] = one | (m & ((_x[4]>> 4)|(_x[5]<<60)));
  id[ 6] = one | (m & ((_x[4]>>56)|(_x[5]<< 8)));
  id[ 7] = one | (m & ((_x[5]>>44)|(_x[6]<<20)));
  id[ 8] = one | (m & ((_x[6]>>32)|(_x[7]<<32)));
  id[ 9] = one | (m & ((_x[7]>>20)|(_x[8]<<44)));
  id[10] = one | (m & _x[8]>>8);

  for(int j=0;j<11;j++) d[j] -= 1;
}

void ranluxpp::getarray(int n, float *a) {
  if(_fpos < 24){ // prologue, if the entropy state is not exhausted fetch first it.
    int rest = 24 - _fpos;
    rest = (rest < n) ? rest : n;
    float *f = (float*)_floats + _fpos;
    for(int i=0;i<rest;i++) a[i] = f[i];
    n     -= rest;
    a     += rest;
    _fpos += rest;
  }
  while(n>=24){
    nextstate();
    unpackfloats(a);
    n -= 24;
    a += 24;
  }
  if(n){
    nextfloats();
    float *f = (float*)_floats;
    for(int i=0;i<n;i++) a[i] = f[i];
    _fpos = n;
  }
}
  
void ranluxpp::getarray(int n, double *a) {
  if(_dpos < 11) {
    int rest = 11 - _dpos;
    rest = (rest < n) ? rest : n;
    double *f = (double*)_doubles + _dpos;
    for(int i=0;i<rest;i++) a[i] = f[i];
    n     -= rest;
    a     += rest;
    _dpos += rest;
  }
  while(n>=11){
    nextstate();
    unpackdoubles(a);
    n -= 11;
    a += 11;
  }
  if(n){
    nextdoubles();
    double *f = (double*)_doubles;
    for(int i=0;i<n;i++) a[i] = f[i];
    _dpos = n;
  }
}

// set the multiplier A to A = a^2048 + 13, a primitive element modulo
// m = 2^576 - 2^240 + 1 to provide the full period m-1 of the sequence.
void ranluxpp::primitive(){
  for(int i=0;i<9;i++) _A[i] = geta()[i];
  powmod(_A, 2048);
  _A[0] += 13;
}

void ranluxpp::init(uint64_t seed){
  uint64_t a[9];
  for(int i=0;i<9;i++) a[i] = _A[i];
  powmod(a, 1UL<<48); powmod(a, 1UL<<48); // skip 2^96 states
  powmod(a, seed); // skip 2^96*seed states
  mul9x9mod(_x, a);
}

// jump ahead by n 24-bit RANLUX numbers
void ranluxpp::jump(uint64_t n){
  uint64_t a[9];
  for(int i=0;i<9;i++) a[i] = geta()[i];
  powmod(a, n);
  mul9x9mod(_x, a);
}

// set skip factor to emulate RANLUX behaviour
void ranluxpp::setskip(uint64_t n){
  for(int i=0;i<9;i++) _A[i] = geta()[i];
  powmod(_A, n);
}
