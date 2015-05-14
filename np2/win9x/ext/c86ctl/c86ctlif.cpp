/**
 * @file	c86ctlif.cpp
 * @brief	G.I.M.I.C アクセス クラスの動作の定義を行います
 */

#include "compiler.h"
#include "c86ctlif.h"
#include "c86ctl.h"

using namespace c86ctl;

//! インタフェイス
typedef HRESULT (WINAPI * FnCreateInstance)(REFIID riid, LPVOID* ppi);

/**
 * コンストラクタ
 */
C86CtlIf::C86CtlIf()
	: m_hModule(NULL)
	, m_chipbase(NULL)
	, m_gimic(NULL)
	, m_chip(NULL)
{
}

/**
 * デストラクタ
 */
C86CtlIf::~C86CtlIf()
{
}

/**
 * 初期化
 * @retval true 成功
 * @retval false 失敗
 */
bool C86CtlIf::Initialize()
{
	Deinitialize();

	do
	{
		// DLL 読み込み
		m_hModule = ::LoadLibrary(TEXT("c86ctl.dll"));
		if (m_hModule == NULL)
		{
			break;
		}
		FnCreateInstance CreateInstance = reinterpret_cast<FnCreateInstance>(::GetProcAddress(m_hModule, "CreateInstance"));
		if (CreateInstance == NULL)
		{
			break;
		}

		// インスタンス作成
		(*CreateInstance)(IID_IRealChipBase, reinterpret_cast< LPVOID*>(&m_chipbase));
		if (m_chipbase == NULL)
		{
			break;
		}

		// 初期化
		if (m_chipbase->initialize() != C86CTL_ERR_NONE)
		{
			break;
		}

		// OPNA を探す
		// @ToDo: YMF-288
		const int nDeviceCount = m_chipbase->getNumberOfChip();
		for (int i = 0; i < nDeviceCount; i++)
		{
			IRealChip* chip = NULL;
			m_chipbase->getChipInterface(i, IID_IRealChip, reinterpret_cast<LPVOID*>(&chip));
			if (chip == NULL)
			{
				continue;
			}

			// G.I.M.I.C 判定
			IGimic* gimic = NULL;
			m_chipbase->getChipInterface(i, IID_IGimic, reinterpret_cast<LPVOID*>(&gimic));
			if (gimic)
			{
				Devinfo info;
				if (gimic->getModuleInfo(&info) == C86CTL_ERR_NONE)
				{
					if ((!memcmp(info.Devname, "GMC-OPN3L", 9)) || (!memcmp(info.Devname, "GMC-OPNA", 8)))
					{
						m_chip = chip;
						m_gimic = gimic;
						Reset();
						return true;
					}
				}
			}

			// その他の判定
			IRealChip3* chip3 = NULL;
			m_chipbase->getChipInterface(i, IID_IRealChip3, reinterpret_cast<LPVOID*>(&chip3));
			if (chip3 != NULL)
			{
				ChipType chiptype = CHIP_UNKNOWN;
				chip3->getChipType(&chiptype);
				const ChipType type = static_cast<ChipType>(chiptype & 0xffff);
				if ((type == CHIP_OPNA) || (type == CHIP_OPN3L))
				{
					m_chip = chip3;
					m_gimic = NULL;
					Reset();
					return true;
				}
			}
		}
	} while (0 /*CONSTCOND*/);

	Deinitialize();
	return false;
}

/**
 * 解放
 */
void C86CtlIf::Deinitialize()
{
	if (m_chipbase)
	{
		m_chipbase->deinitialize();
		m_chipbase = NULL;
		m_gimic = NULL;
		m_chip = NULL;
	}
	if (m_hModule)
	{
		::FreeLibrary(m_hModule);
		m_hModule = NULL;
	}
}

/**
 * デバイスは有効?
 * @retval true 有効
 * @retval false 無効
 */
bool C86CtlIf::IsEnabled()
{
	return (m_chipbase != NULL);
}

/**
 * ビジー?
 * @retval true ビジー
 * @retval false レディ
 */
bool C86CtlIf::IsBusy()
{
	return false;
}

/**
 * 音源リセット
 */
void C86CtlIf::Reset()
{
	if (m_chip)
	{
		m_chip->reset();
	}
	if (m_gimic)
	{
		m_gimic->setPLLClock(7987200);
		m_gimic->setSSGVolume(31);
	}
}

/**
 * レジスタ書き込み
 * @param[in] nAddr アドレス
 * @param[in] cData データ
 */
void C86CtlIf::WriteRegister(UINT nAddr, UINT8 cData)
{
	if (m_chip)
	{
		m_chip->out(nAddr, cData);
	}
}
