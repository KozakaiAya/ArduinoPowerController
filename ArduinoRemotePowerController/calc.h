#pragma once

inline Uint64 addm(const Uint64& a, const Uint64& b, const Uint64& c)
{
	Uint64 al = a & 0xFFFFFFFFuLL;
	Uint64 ah = a >> 32uLL;
	Uint64 bl = b & 0xFFFFFFFFuLL;
	Uint64 bh = b >> 32uLL;
	Uint64 lowSum = al + bl;
	Uint64 lowCarry = lowSum >> 32uLL;
	lowSum = lowSum & 0xFFFFFFFFuLL;
	Uint64 highSum = lowCarry + ah + bh;
	Uint64 highCarry = highSum >> 32uLL;
	highSum = highSum & 0xFFFFFFFFuLL;
	Uint64 sum = lowSum + (highSum << 32uLL);
	if (highCarry)
	{
		Uint64 lowBit = sum & 1uLL;
		sum = ((sum >> 1) | 0x8000000000000000uLL) % c;
		Uint64 c2 = c >> 1;
		if (sum > c2) sum = ((sum - c2 - 1) << 1) + (c & 1);
		else sum <<= 1;
		sum += lowBit;
	}
	return sum;
}

inline Uint64 shiftlm(Uint64 a, const Uint64& c)
{
	a = a%c;
	if (a & 0x8000000000000000uLL)
	{
		Uint64 c2 = c >> 1;
		if (a > c2) return ((a - c2 - 1) << 1) + (c & 1);
		else return a << 1;
	}
	else
	{
		return a << 1;
	}
}

inline Uint64 mulm(Uint64 a, Uint64 b, Uint64 c)
{
	Uint64 p = 0;
	Uint64 n = 1;
	while (b)
	{
		if (b & 1) p = addm(p, a, c);
		a = shiftlm(a, c);
		b >>= 1;
	}
	return p;
}

inline Uint64 log2(Uint64 a)
{
	Uint64 r = 0;
	if (a & 0x2uLL)
	{
		a >>= 1;
		r |= 1;
	}
	if (a & 0xCuLL)
	{
		a >>= 2;
		r |= 2;
	}
	if (a & 0xF0uLL)
	{
		a >>= 4;
		r |= 4;
	}
	if (a & 0xFF00uLL)
	{
		a >>= 8;
		r |= 8;
	}
	if (a & 0xFFFF0000uLL)
	{
		a >>= 16;
		r |= 16;
	}
	if (a & 0xFFFFFFFF00000000uLL)
	{
		a >>= 32;
		r |= 32;
	}
	return r;
}

Uint64 powm(Uint64 b, Uint64 a, Uint64 m)
{
	Uint64 result = 1;
	int start = millis();
	int k = 1;
	while (a)
	{
		int t = millis() - start;
		if (t > WAIT_INTERVAL*k)
		{
			k++;
			Serial.println("Sending wait.");
			Udp.beginPacket(remoteIP, remotePort);
			Udp.write(udpBuffer, MESSAGE_SIZE);
			Udp.endPacket();
		}
		if (a & 1) result = mulm(result, b, m);
		b = mulm(b, b, m);
		a >>= 1;
	}
	return result;
}
