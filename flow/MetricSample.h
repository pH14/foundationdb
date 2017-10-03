/*
 * MetricSample.h
 *
 * This source file is part of the FoundationDB open source project
 *
 * Copyright 2013-2018 Apple Inc. and the FoundationDB project authors
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef METRIC_SAMPLE_H
#define METRIC_SAMPLE_H
#pragma once

template <class T>
struct MetricSample {
	IndexedSet<T, int64_t> sample;
	int64_t metricUnitsPerSample = 0;
	
	explicit MetricSample(int64_t metricUnitsPerSample) : metricUnitsPerSample(metricUnitsPerSample) {}

	int64_t getMetric(const T& Key) {
		auto i = sample.find(Key);
		if (i == sample.end())
			return 0;
		else
			return sample.getMetric(i);
	}	
};

template <class T>
struct TransientMetricSample : MetricSample<T> {
	Deque< std::pair<double, std::pair<T, int64_t>> > queue;

	explicit TransientMetricSample(int64_t metricUnitsPerSample) : MetricSample<T>(metricUnitsPerSample) { }

	bool roll(int64_t metric) {
		return g_random->random01() < (double)metric / this->metricUnitsPerSample;	//< SOMEDAY: Better randomInt64?
	}

	// Returns the sampled metric value (possibly 0, possibly increased by the sampling factor)
	int64_t addAndExpire(const T& key, int64_t metric, double expiration) {
		int64_t x = add(key, metric);
		if (x)
			queue.push_back(std::make_pair(expiration, std::make_pair(*this->sample.find(key), -x)));
		return x;
	}

	void poll() {
		double now = ::now();
		while (queue.size() &&
			queue.front().first <= now)
		{
			const T& key = queue.front().second.first;
			int64_t delta = queue.front().second.second;
			ASSERT(delta != 0);

			if (this->sample.addMetric(T(key), delta) == 0)
				this->sample.erase(key);

			queue.pop_front();
		}
	}

private:
	int64_t add(const T& key, int64_t metric) {
		if (!metric) return 0;
		int64_t mag = metric<0 ? -metric : metric;

		if (mag < this->metricUnitsPerSample) {
			if (!roll(mag))
				return 0;

			metric = metric<0 ? -this->metricUnitsPerSample : this->metricUnitsPerSample;
		}

		if (this->sample.addMetric(T(key), metric) == 0)
			this->sample.erase(key);

		return metric;
	}
};
#endif
