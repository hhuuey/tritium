"""最小协议接入示例；算法自行读取配置和生成业务产物。"""

import argparse
from pathlib import Path

from asset_protocol import AssetError, read_request_identity, read_request, write_result, failure_result, success_result


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--request", type=Path, required=True)
    parser.add_argument("--result", type=Path, required=True)
    parser.add_argument("--fail", action="store_true")
    args = parser.parse_args()
    # 身份无效时不能生成可信结果；其余解析或业务错误使用相同身份返回失败。
    identity = read_request_identity(args.request)
    try:
        request = read_request(args.request)
        if args.fail:
            raise RuntimeError("示例主动返回失败")
        result = success_result(request, metrics={"example": 1})
    except Exception as error:
        write_result(args.result, failure_result(
            identity, AssetError("algorithm", "EXAMPLE_FAILURE", str(error))
        ))
        raise SystemExit(1) from error
    write_result(args.result, result)


if __name__ == "__main__":
    main()
