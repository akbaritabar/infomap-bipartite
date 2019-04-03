/**********************************************************************************

 Infomap software package for multi-level network clustering

 Copyright (c) 2013, 2014 Daniel Edler, Martin Rosvall
 
 For more information, see <http://www.mapequation.org>
 

 This file is part of Infomap software package.

 Infomap software package is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Infomap software package is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with Infomap software package.  If not, see <http://www.gnu.org/licenses/>.

**********************************************************************************/


#ifndef INFOMATH_H_
#define INFOMATH_H_

#include <cmath>
#include <vector>

namespace infomap
{
	namespace infomath
	{
		inline double total(std::pair<double, double> p)
		{
			return p.first + p.second;
		}

		inline
		double log2(double p)
		{
			return std::log(p) * M_LOG2E; // M_LOG2E == 1 / M_LN2
		}

		inline
		double plogp(double p)
		{
			return p > 0.0 ? p * log2(p) : 0.0;
		}

		inline
		std::pair<double, double> plogp(std::pair<double, double> p)
		{
			return std::make_pair(p.first  > 0.0 ? p.first * log2(p.first) : 0.0, p.second > 0.0 ? p.second * log2(p.second) : 0.0);
		}

		inline
		double plogq(double p, double q)
		{
			return q > 0.0 ? p * log2(q) : 0.0;
		}

		inline
		double plogpN(unsigned int p, unsigned int N)
		{
			return p > 0 ? 1.0 / N * (plogp(p) - plogq(p, N)) : 0.0;
		}

		template<typename T, typename U>
		inline
		bool isBetween(T value, U lowerLimit, U higherLimit)
		{
			return value >= lowerLimit && value <= higherLimit;
		}

		template<typename T, typename U>
		inline
		bool isBetweenStrict(T value, U lowerLimit, U higherLimit)
		{
			return value > lowerLimit && value < higherLimit;
		}
		
		inline
		bool isEqualWithinThreshold(double v1, double v2, double epsilon = 1e-10)
		{
			return std::abs(v2 - v1) < epsilon;
		}
	}
}
#endif /* INFOMAPTH_H_ */
