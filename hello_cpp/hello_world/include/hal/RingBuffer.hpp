#pragma once

#include <zephyr/sys/ring_buffer.h>

namespace RTOS
{
	namespace HAL
	{
		class RingBuffer256u8 {
		public:
			RingBuffer256u8() {
				ring_buf_init(&mHandler, 256, mBuffer);
			}

			bool IsEmpty() {
				return ring_buf_is_empty(&mHandler);
			}

			void Reset() {
				ring_buf_reset(&mHandler);
			}

			uint32_t Space() {
				return ring_buf_space_get(&mHandler);
			}

			uint32_t Capacity() {
				return ring_buf_capacity_get(&mHandler);
			}

			uint32_t Size() {
				return ring_buf_size_get(&mHandler);
			}

			uint32_t Peek(uint8_t* data, uint32_t size) {
				return ring_buf_peek(&mHandler, data, size);
			}

			uint32_t Put(const uint8_t* data, uint32_t& size) {
				return ring_buf_put(&mHandler, data, size);
			}

			uint32_t Get(uint8_t* data, uint32_t size) {
				return ring_buf_get(&mHandler, data, size);
			}

		private:
			struct ring_buf mHandler;
			uint8_t mBuffer[256];
		};


		template<class T, size_t N>
		class RingBuffer {
		public:
			RingBuffer() {
				ring_buf_init(&mHandler, N*sizeof(T), reinterpret_cast<uint8_t*>(mBuffer));
			}

			bool IsEmpty() const {
				return ring_buf_is_empty(&mHandler);
			}

			void Reset() {
				ring_buf_reset(&mHandler);
			}

			uint32_t Space() const {
				return ring_buf_space_get(&mHandler);
			}

			uint32_t Capacity() const {
				return ring_buf_capacity_get(&mHandler);
			}

			uint32_t Size() const {
				return ring_buf_size_get(&mHandler);
			}

			uint32_t Peek(uint8_t* data, uint32_t& size) {
				return ring_buf_peek(&mHandler, data, size);
			}

			uint32_t Put(const T* data, uint32_t& size) {
				return ring_buf_put(&mHandler, reinterpret_cast<const uint8_t*>(data),  size*sizeof(T));
			}

			uint32_t Get(T* data, uint32_t& size) {
				return ring_buf_get(&mHandler, reinterpret_cast<uint8_t*>(data),  size*sizeof(T));
			}

		private:
			struct ring_buf mHandler;
			T mBuffer[N];
		};
	};
};