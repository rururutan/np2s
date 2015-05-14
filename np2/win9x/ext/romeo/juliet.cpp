/**
 * @file	juliet.cpp
 * @brief	ROMEO アクセス クラスの動作の定義を行います
 */

#include "compiler.h"
#include "juliet.h"
#include <stddef.h>
#include "romeo.h"

//! DLL 名
#define	PCIDEBUG_DLL			TEXT("pcidebug.dll")

//! PCI デバイス アドレスを作成
#define PCIBUSDEVFUNC(b, d, f)	(((b) << 8) | ((d) << 3) | (f))

//! ベンダ/デバイスを作成
#define	DEVVEND(v, d)			((ULONG)((v) | ((d) << 16)))

/**
 * コンストラクタ
 */
CJuliet::CJuliet()
	: m_hModule(NULL)
	, m_fnRead32(NULL)
	, m_fnOut8(NULL)
	, m_fnOut32(NULL)
	, m_fnIn8(NULL)
	, m_ulAddress(0)
	, m_ucIrq(0)
{
}

/**
 * デストラクタ
 */
CJuliet::~CJuliet()
{
}

/**
 * 初期化
 * @retval true 成功
 * @retval false 失敗
 */
bool CJuliet::Initialize()
{
	Deinitialize();

	m_hModule = ::LoadLibrary(PCIDEBUG_DLL);
	if (m_hModule == NULL)
	{
		return false;
	}

	//! ロード関数リスト
	static const ProcItem s_dllProc[] =
	{
		{"_pciConfigReadLong",	offsetof(CJuliet, m_fnRead32)},
		{"_MemWriteChar",		offsetof(CJuliet, m_fnOut8)},
		{"_MemWriteLong",		offsetof(CJuliet, m_fnOut32)},
		{"_MemReadChar",		offsetof(CJuliet, m_fnIn8)},
	};

	for (size_t i = 0; i < _countof(s_dllProc); i++)
	{
		FARPROC proc = ::GetProcAddress(m_hModule, s_dllProc[i].lpSymbol);
		if (proc == NULL)
		{
			Deinitialize();
			return false;
		}
		*(reinterpret_cast<FARPROC*>(reinterpret_cast<INT_PTR>(this) + s_dllProc[i].nOffset)) = proc;
	}

	const ULONG ulPciAddress = SearchRomeo();
	if (ulPciAddress == static_cast<ULONG>(-1))
	{
		Deinitialize();
		return false;
	}
	m_ulAddress = (*m_fnRead32)(ulPciAddress, ROMEO_BASEADDRESS1);
	m_ucIrq = static_cast<UCHAR>((*m_fnRead32)(ulPciAddress, ROMEO_PCIINTERRUPT));
	if (!m_ulAddress)
	{
		Deinitialize();
		return false;
	}

	Reset();

	return true;
}

/**
 * 解放
 */
void CJuliet::Deinitialize()
{
	if (m_hModule)
	{
		::FreeLibrary(m_hModule);
	}
	m_hModule = NULL;
	m_fnRead32 = NULL;
	m_fnOut8 = NULL;
	m_fnOut32 = NULL;
	m_fnIn8 = NULL;
	m_ulAddress = 0;
	m_ucIrq = 0;
}

/**
 * ROMEO を探す
 * @note pciFindPciDevice使うと、OS起動後一発目に見つけられないことが多いので、自前で検索する（矢野さん方式）
 * @return バス アドレス
 */
ULONG CJuliet::SearchRomeo() const
{
	for (UINT bus = 0; bus < 0x100; bus++)
	{
		for (UINT dev = 0; dev < 0x20; dev++)
		{
			for (UINT func = 0; func < 0x08; func++)
			{
				const ULONG ulPciAddress = PCIBUSDEVFUNC(bus, dev, func);
				const ULONG ulDeviceVendor = (*m_fnRead32)(ulPciAddress, 0x0000);
				if ((ulDeviceVendor == DEVVEND(ROMEO_VENDORID, ROMEO_DEVICEID)) || (ulDeviceVendor == DEVVEND(ROMEO_VENDORID, ROMEO_DEVICEID2)))
				{
					return ulPciAddress;
				}
			}
		}
	}
	return static_cast<ULONG>(-1);
}

/**
 * ビジー?
 * @retval true ビジー
 * @retval false レディ
 */
bool CJuliet::IsBusy()
{
	return (m_fnIn8 == NULL) || (((*m_fnIn8)(m_ulAddress + ROMEO_YMF288ADDR1) & 0x80) != 0);

}

/**
 * 音源リセット
 */
void CJuliet::Reset()
{
	if (m_fnOut32 != NULL)
	{
		(*m_fnOut32)(m_ulAddress + ROMEO_YMF288CTRL, 0x00);
		::Sleep(150);

		(*m_fnOut32)(m_ulAddress + ROMEO_YMF288CTRL, 0x80);
		::Sleep(150);
	}
}

/**
 * レジスタ書き込み
 * @param[in] nAddr アドレス
 * @param[in] cData データ
 */
void CJuliet::WriteRegister(UINT nAddr, UINT8 cData)
{
	if (m_ulAddress != 0)
	{
		while (((*m_fnIn8)(m_ulAddress + ROMEO_YMF288ADDR1) & 0x80) != 0)
		{
			::Sleep(0);
		}
		(*m_fnOut8)(m_ulAddress + ((nAddr & 0x100) ? ROMEO_YMF288ADDR2 : ROMEO_YMF288ADDR1), nAddr);

		while (((*m_fnIn8)(m_ulAddress + ROMEO_YMF288ADDR1) & 0x80) != 0)
		{
			::Sleep(0);
		}
		(*m_fnOut8)(m_ulAddress + ((nAddr & 0x100) ? ROMEO_YMF288DATA2 : ROMEO_YMF288DATA1), cData);
	}
}
