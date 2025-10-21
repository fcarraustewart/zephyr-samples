#ifndef ACTIVE_OBJECT__H_H
#define ACTIVE_OBJECT__H_H

#include <hal/RTOS.hpp>
#include <zephyr/logging/log.h>
// LOG_MODULE_REGISTER(ActiveObject, CONFIG_LOG_MAX_LEVEL);
#include <zephyr/zbus/zbus.h>
#include <initializer_list>
#include <vector>
//#include <Logger.hpp>

// Channels
ZBUS_CHAN_DECLARE(acc_data_chan);
ZBUS_CHAN_DECLARE(controls_chan);

namespace RTOS
{
	// Work in progress: adding concepts to the ActiveObject
	template <typename T>
	concept StaticBinding_Impl = requires (T t) 
	{ 
		{ t.Initialize () } -> std::same_as < void >;
		{ t.Loop () } -> std::same_as < void >;
		{ t.Handle () } -> std::same_as < void >;
		{ t.End () } -> std::same_as < void >;
	};

    template <class D>
    class ActiveObject
    {
    public:
		constexpr ActiveObject() {Create();};
		
		~ActiveObject() = default;

        static constexpr bool Create()
        {
			auto res = mHandle.set_name(mName);
			if(res.has_value() == false)
				return false;
            const struct zbus_channel* chans[] = {&acc_data_chan, &controls_chan};
            for (auto* chan : chans) {
                if (!chan) {
                    // LOG_WRN("Null channel in init()");
                    continue;
                }
                int ret = zbus_chan_add_obs(chan, ActiveObject::mSub, K_NO_WAIT);
                if (ret != 0) {
                    // LOG_ERR("Failed to subscribe to channel %s, err=%d",
                    //         zbus_chan_name(chan), ret);
                } else {
                    // LOG_INF("Subscribed to channel %s", zbus_chan_name(chan));
                }
            }

            RTOS::Hal::TaskCreate(&SubscriberTask, "mName", &mZbusHandle);

			return RTOS::Hal::TaskCreate(&Run, mName, &mHandle);
        };
        static constexpr void Run(void) noexcept
        {
            D::Initialize();

            while(1)
                Loop();

            D::End();
        };
        static constexpr void SubscriberTask(void) noexcept
        {
            const struct zbus_channel* chan;
            const struct zbus_channel* chans[] = {&acc_data_chan, &controls_chan};
            uint8_t buffer[65]; // generic buffer
        
            while (!zbus_sub_wait_msg(ActiveObject::mSub, &chan, &buffer[1], K_FOREVER)) {
                uint8_t Cmd = chan == chans[0] ?        0x98 
                            : chan == chans[1] ?        0x99 
                            : 0;
                buffer[0] = Cmd;
                Send(buffer);
            }
        };
        static constexpr void Loop()
        {
            if(true == RTOS::Hal::QueueReceive(&mInputQueue, (void*)mReceivedMsg))
                D::Handle(mReceivedMsg);
        };
        static inline void Send(const uint8_t msg[])
        {
            RTOS::Hal::QueueSend(&mInputQueue, msg);
        };
    private:
    protected:

    /**
     *                  Member Variables:
     */
    public:
        static const    char         	mName[];                    /**< The variables used to create the queue */
        static const    uint8_t         mInputQueueItemSize;        /**< The variables used to create the queue */
    protected:
        static          uint8_t         mCountLoops;                /**< The variables used to create the queue */
        static const    uint8_t         mInputQueueItemLength;      /**< The variables used to create the queue */

        
        static const    size_t          mInputQueueSizeBytes;       /**< The variables used to create the queue */
        static          char            mInputQueueAllocation[];    /**< The variables used to create the queue */

        /** 
         * The variable used to hold the queue's data structure. 
         * */
        static          RTOS::QueueHandle_t   mInputQueue;

        /** 
         * The handle used to hold the task for this ActiveObject. 
         * */
        static          RTOS::TaskHandle_t  mHandle;
        static          RTOS::TaskHandle_t  mZbusHandle;
        static          uint8_t             mReceivedMsg[];
        
        static          zpp::thread_stack   mTaskStack;
        static          zpp::thread_data    mTaskControlBlock;
        static          zpp::thread_data    mZbusControlBlock;
    public:
        static          const struct zbus_observer* mSub;


    };

}
#endif
