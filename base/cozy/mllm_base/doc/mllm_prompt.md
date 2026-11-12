现在需要将mllm泛化成一个后续容易集成使用的类
后续项目提供config.py， dataset.py既可以使用mllm进行推理
1. config.py包含以下内容
  - model_pt_path 模型的路径
  - output_datastructure:  一个输出类，继承Pydantic定义一个输出的数据结构及相关注释
  - 最大重试次数（输出格式验证失败，文件图片读取失败等）
  - batch_size: 20
  - output_path: 最终结果的输出路径
2. dataset.py继承自mllm_dataset

希望通用的mllm_base下面的文件目录包含以下结构

-- mllm_infer
一个类，内部附带AutoModelForVision2Seq，初始化提供model_pt_path，并对外暴露infer及infer_batch两个接口
-- mllm_dataset, 继承自torchnn中的dataset
一个类，内部附带AutoProcessor, 将dataset中的prompt text及images转化成model的input，并包含一个save_sample_result()函数
同时提供DataLoader以及collate_fn
-- mllm_verify
- 模型输出格式验证器，写一个类，定义格式Pydantic，验证输出
-- mllm_retry_manager
- 失败样本重试器， 对失败的样本进行重试推理

-- mllm_infer_engine
通用的mllm类，最终对外暴露的接口.  最终数据流dataloader --> mllm_infer  infer_batch --> mllm_verify --> 验证通过的save_sample_result()，不通过的submit到mllm_retry_manager， 当最终处理完成后，再对失败样本进行重试推理。