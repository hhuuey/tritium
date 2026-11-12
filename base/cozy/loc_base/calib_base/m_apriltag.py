import cv2
import m_apriltag

# 选择要生成的Apriltag的类型
tag_family = 'tagStandard41h12'

# 创建Apriltag生成器
generator = apriltag.DetectorSearchGabe(apriltag.Family.fromString(tag_family))

# 生成Apriltag图像
tag_id = 0
tag_size = 200
border_size = 1
img = generator.drawMarker(tag_id, tag_size, border_size)

# 显示生成的Apriltag图像
cv2.imshow('Apriltag', img)
cv2.waitKey(0)
cv2.destroyAllWindows()