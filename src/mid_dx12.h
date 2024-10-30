#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOCOMM
#include <windows.h>

#define COBJMACROS
#include <d3d12.h>
#include <dxgi1_6.h>

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef MID_DEBUG
#define MID_DEBUG
extern void Panic(const char* file, int line, const char* message);
#define PANIC(_message) Panic(__FILE__, __LINE__, _message)
#define CHECK(_err, _message)                      \
	if (__builtin_expect(!!(_err), 0)) {           \
		fprintf(stderr, "Error Code: %d\n", _err); \
		PANIC(_message);                           \
	}
#ifdef MID_DX12_IMPLEMENTATION
[[noreturn]] void Panic(const char* file, int line, const char* message)
{
	fprintf(stderr, "\n%s:%d Error! %s\n", file, line, message);
	__builtin_trap();
}
#endif
#endif

#ifndef MID_WIN32_DEBUG
#define MID_WIN32_DEBUG
static void LogWin32Error(HRESULT err)
{
	fprintf(stderr, "Win32 Error Code: 0x%08lX\n", err);
	char* errStr;
	if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
					  NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), (LPSTR)&errStr, 0, NULL)) {
		fprintf(stderr, "%s\n", errStr);
		LocalFree(errStr);
	}
}
#define CHECK_WIN32(condition, err)          \
	if (__builtin_expect(!(condition), 0)) { \
		LogWin32Error(err);                  \
		PANIC("Win32 Error!");               \
	}
#endif

#define DX_CHECK(command)               \
	({                                  \
		HRESULT hr = command;           \
		CHECK_WIN32(SUCCEEDED(hr), hr); \
	})

struct {
	IDXGIFactory4* factory;
	IDXGIAdapter1* adapter;
	ID3D12Device*  device;
} d3d12;

// This is all could go mid_vulkan as a way to make cross platform textures

void CreateDX12Texture(ID3D12Resource* pTexture)
{
	UINT flags = 0;
#if BUILD_DEBUG
	flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

	DX_CHECK(CreateDXGIFactory2(flags, &IID_IDXGIFactory4, (void**)&d3d12.factory));
	for (UINT i = 0; IDXGIFactory1_EnumAdapters1(d3d12.factory, i, &d3d12.adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC1 desc;
		DX_CHECK(IDXGIAdapter1_GetDesc1(d3d12.adapter, &desc));
		wprintf(L"DX12 Adapter %d Name: %ls Description: %ld:%lu\n",
				i, desc.Description, desc.AdapterLuid.HighPart,	desc.AdapterLuid.LowPart);
		break; // add logic to choose?
	}

	DX_CHECK(D3D12CreateDevice((IUnknown*)d3d12.adapter, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void**)&d3d12.device));

	D3D12_RESOURCE_DESC textureDesc = {
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		.Alignment = 0,
		.Width = 512,
		.Height = 512,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.SampleDesc.Count = 1,
		.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS,
	};
	D3D12_HEAP_PROPERTIES heapProperties = {
		.Type = D3D12_HEAP_TYPE_DEFAULT,
	};
	DX_CHECK(ID3D12Device_CreateCommittedResource(
		d3d12.device,
		&heapProperties,
		D3D12_HEAP_FLAG_SHARED,
		&textureDesc,
		D3D12_RESOURCE_STATE_COMMON,
		NULL,
		&IID_ID3D12Resource,
		(void**)pTexture));

	IDXGIFactory4_Release(d3d12.factory);
	IDXGIAdapter1_Release(d3d12.adapter);
	ID3D12Device_Release(d3d12.device);
}