from dataclasses import dataclass, field
from enum import Enum
from typing import Any, List


## 关于MLLM数据集的定义，目前主要包含以下几个类：
class Role(str, Enum):
    system = 'system'
    user = 'user'
    assistant = 'assistant'


class ContentType(str, Enum):
    text = 'text'
    image = 'image'


@dataclass
class Content:
    type: ContentType
    data: Any


@dataclass
class Message:
    role: Role = Role.user
    content: List[Content] = field(default_factory=list)

    def to_dict(self) -> dict:
        return {
            'role': self.role.value,
            'content': [{
                'type': content.type.value,
                content.type.value: content.data,
            } for content in self.content],
        }

    @staticmethod
    def from_text(text: str, role: Role = Role.user) -> "Message":
        return Message(
            role=role,
            content=[Content(type=ContentType.text, data=text)],
        )

    @staticmethod
    def from_image(image_data: Any, role: Role = Role.user) -> "Message":
        return Message(
            role=role,
            content=[Content(type=ContentType.image, data=image_data)],
        )

    @staticmethod
    def from_mixed(image_data: Any, text: str, role: Role = Role.user) -> "Message":
        return Message(
            role=role,
            content=[
                Content(type=ContentType.image, data=image_data),
                Content(type=ContentType.text, data=text),
            ],
        )


# 关于embedding数据集的定义，目前主要包含以下几个类：
@dataclass
class Document:
    type: ContentType
    data: Any

    def to_dict(self) -> dict:
        return {
            self.type.value: self.data,
        }

    @staticmethod
    def from_text(text: str) -> "Document":
        return Document(
            type=ContentType.text,
            data=text,
        )

    @staticmethod
    def from_image(image_data: Any) -> "Document":
        return Document(
            type=ContentType.image,
            data=image_data,
        )


@dataclass
class Query:
    type: ContentType
    data: Any
    instuct: str

    def to_dict(self) -> dict:
        return {
            self.type.value: self.data,
            'instruction': self.instuct,
        }

    @staticmethod
    def from_text(text: str, instruction: str) -> "Query":
        return Query(
            type=ContentType.text,
            data=text,
            instuct=instruction,
        )

    @staticmethod
    def from_image(image_data: Any, instruction: str) -> "Query":
        return Query(
            type=ContentType.image,
            data=image_data,
            instuct=instruction,
        )
