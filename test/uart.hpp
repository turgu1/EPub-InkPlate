#ifndef __UART_HPP__
#define __UART_HPP__

#define EX_UART_NUM UART_NUM_0
#define PATTERN_CHR_NUM    (3)         /*!< Set the number of consecutive and identical characters received by receiver which defines a UART pattern*/

#define BUF_SIZE (32)
#define RD_BUF_SIZE (BUF_SIZE)

class Uart
{
public:

	Uart() {};
	~Uart() {};

private:

	static constexpr char const * TAG = "Uart";
  
	static void uart_event_task(void * pvParameters);

public:
	void init();
};

#if __UART__
  Uart uart;
#else
  extern Uart uart;
#endif

#endif